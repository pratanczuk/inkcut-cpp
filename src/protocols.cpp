// SPDX-License-Identifier: GPL-3.0-or-later

#include "protocols.hpp"

#include <QByteArray>
#include <QtGlobal>

#include <cmath>
#include <sstream>

namespace inkcut {

PlotStreamEncoder::PlotStreamEncoder(std::function<void(std::string)> sink, ProtocolSettings settings)
    : sink_(std::move(sink)), s_(std::move(settings))
{
}

void PlotStreamEncoder::set_initial_pen_up(bool pen_up)
{
    gcode_currently_up_ = pen_up;
}

void PlotStreamEncoder::write_payload(std::string data)
{
    if (s_.protocol == PlotProtocol::HPGL && s_.hpgl_pad)
        data.push_back('\n');
    sink_(std::move(data));
}

void PlotStreamEncoder::connection_made()
{
    switch (s_.protocol) {
    case PlotProtocol::HPGL:
        write_payload("IN;");
        break;
    case PlotProtocol::DMPL: {
        const int v = s_.dmpl_mode;
        if (v == 1)
            write_payload(";:HAEC1");
        else if (v == 2)
            write_payload(" ;:ECN A L0 ");
        else if (v == 3 || v == 4)
            write_payload(" ;:H A L0 ");
        else if (v == 6)
            write_payload("IN;PA;");
        break;
    }
    case PlotProtocol::GPGL:
        write_payload(std::string{'\x03', '\x03', 'H'});
        break;
    case PlotProtocol::GCode:
        if (s_.gcode.use_builtin) {
            if (s_.gcode.dialect == GCodeProtocolSettings::Dialect::Grbl) {
                write_payload("G21; mm\n");
                write_payload("G90; absolute\n");
                if (s_.gcode.lift_mode == GCodeProtocolSettings::ZAxis) {
                    const int prec = s_.gcode.precision;
                    write_payload(
                        QStringLiteral("G0 Z%1; pen up\n")
                            .arg(s_.gcode.upper_z, 0, 'f', prec)
                            .toStdString());
                }
            } else {
                write_payload("G28; Return to home\n");
                write_payload("G98; Return to initial z\n");
                write_payload("G90; Use absolute coordinates\n");
            }
        }
        if (s_.gcode.lift_mode == GCodeProtocolSettings::SolenoidPwm) {
            const int up = s_.gcode.solenoid_pwm_up;
            if (up <= 0)
                write_payload("M5; pen up (solenoid off)\n");
            else
                write_payload(
                    QStringLiteral("M3 S%1; pen up\n").arg(up).toStdString());
            gcode_currently_up_ = true;
        }
        break;
    case PlotProtocol::CAMM_GL1:
        write_payload("IN;");
        break;
    }
}

void PlotStreamEncoder::connection_lost()
{
}

void PlotStreamEncoder::move_hpgl(double x, double y, double z, bool absolute)
{
    const int ix = static_cast<int>(std::lround(x * s_.plot_scale));
    const int iy = static_cast<int>(std::lround(y * s_.plot_scale));
    std::ostringstream os;
    if (absolute)
        os << (z != 0.0 ? "PD" : "PU") << ix << ',' << iy << ';';
    else
        os << "PR" << ix << ',' << iy << ';';
    write_payload(os.str());
}

void PlotStreamEncoder::move_dmpl(double x, double y, double z, bool absolute)
{
    Q_UNUSED(absolute);
    const int ix = static_cast<int>(std::lround(x * s_.plot_scale));
    const int iy = static_cast<int>(std::lround(y * s_.plot_scale));
    const int v = s_.dmpl_mode;
    std::ostringstream os;
    if (v >= 1 && v <= 4)
        os << ' ' << (z != 0.0 ? 'D' : 'U') << ix << ',' << iy << ' ';
    else
        os << (z != 0.0 ? "PD" : "PU") << ix << ',' << iy << ';';
    write_payload(os.str());
}

void PlotStreamEncoder::move_gpgl(double x, double y, double z, bool absolute)
{
    Q_UNUSED(absolute);
    const int ix = static_cast<int>(std::lround(x));
    const int iy = static_cast<int>(std::lround(y));
    std::ostringstream os;
    os << '\x03' << (z != 0.0 ? 'D' : 'M') << ix << ',' << iy;
    write_payload(os.str());
}

void PlotStreamEncoder::send_gcode_block(const QString& commands)
{
    if (commands.isEmpty())
        return;
    QByteArray b = commands.toUtf8();
    if (!b.endsWith('\n'))
        b.append('\n');
    write_payload(std::string(b.constData(), static_cast<size_t>(b.size())));
}

void PlotStreamEncoder::send_command_block(const QString& commands)
{
    if (commands.isEmpty())
        return;
    QString payload = commands;
    payload.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
    payload.replace(QStringLiteral("\\r"), QStringLiteral("\r"));
    if (s_.protocol == PlotProtocol::GCode)
        send_gcode_block(payload);
    else
        write_payload(payload.toUtf8().constData());
}

void PlotStreamEncoder::move_gcode(double x, double y, double z, bool absolute)
{
    Q_UNUSED(absolute);
    const bool pen_up = (z == 0.0);
    if (gcode_currently_up_ != pen_up) {
        if (gcode_currently_up_) {
            if (s_.gcode.lift_mode == GCodeProtocolSettings::Custom)
                send_gcode_block(s_.gcode.lower_gcode);
            else if (s_.gcode.lift_mode == GCodeProtocolSettings::SolenoidPwm) {
                const int down = s_.gcode.solenoid_pwm_down;
                send_gcode_block(QStringLiteral("M3 S%1\n").arg(down));
            }
        } else {
            if (s_.gcode.lift_mode == GCodeProtocolSettings::Custom)
                send_gcode_block(s_.gcode.lift_gcode);
            else if (s_.gcode.lift_mode == GCodeProtocolSettings::SolenoidPwm) {
                const int up = s_.gcode.solenoid_pwm_up;
                send_gcode_block(up <= 0 ? QStringLiteral("M5\n")
                                         : QStringLiteral("M3 S%1\n").arg(up));
            }
        }
        gcode_currently_up_ = pen_up;
    }

    const double scale = 1.0;
    const double sx = x * scale;
    const double sy = y * scale;
    const int prec = s_.gcode.precision;
    const bool grbl = s_.gcode.dialect == GCodeProtocolSettings::Dialect::Grbl;
    const char* move_cmd = grbl ? (pen_up ? "G0" : "G1") : ((z != 0.0) ? "G01" : "G00");

    QString line = QStringLiteral("%1 X%2 Y%3")
                         .arg(QLatin1String(move_cmd))
                         .arg(sx, 0, 'f', prec)
                         .arg(sy, 0, 'f', prec);

    if (s_.gcode.lift_mode == GCodeProtocolSettings::ZAxis) {
        const double physical_z = (z != 0.0) ? s_.gcode.lower_z : s_.gcode.upper_z;
        line += QStringLiteral(" Z%1").arg(physical_z, 0, 'f', prec);
    }
    const int feed = pen_up ? s_.gcode.feed_rapid_mm_min : s_.gcode.feed_mm_min;
    if (feed > 0)
        line += QStringLiteral(" F%1").arg(feed);
    line += QLatin1Char('\n');
    send_gcode_block(line);
}

void PlotStreamEncoder::move_camm(double x, double y, double z, bool absolute)
{
    Q_UNUSED(absolute);
    std::ostringstream os;
    os << (z != 0.0 ? 'D' : 'M') << x << ',' << y << ';';
    write_payload(os.str());
}

void PlotStreamEncoder::move(double x, double y, double z, bool absolute)
{
    switch (s_.protocol) {
    case PlotProtocol::HPGL:
        move_hpgl(x, y, z, absolute);
        break;
    case PlotProtocol::DMPL:
        move_dmpl(x, y, z, absolute);
        break;
    case PlotProtocol::GPGL:
        move_gpgl(x, y, z, absolute);
        break;
    case PlotProtocol::GCode:
        move_gcode(x, y, z, absolute);
        break;
    case PlotProtocol::CAMM_GL1:
        move_camm(x, y, z, absolute);
        break;
    }
}

void PlotStreamEncoder::set_force(int f)
{
    switch (s_.protocol) {
    case PlotProtocol::HPGL:
        write_payload("FS" + std::to_string(f) + "; ");
        break;
    case PlotProtocol::DMPL:
        write_payload("BP" + std::to_string(f) + ' ');
        break;
    case PlotProtocol::GPGL:
        write_payload(std::string{'\x03', 'F', 'X'} + std::to_string(f) + ",1");
        break;
    case PlotProtocol::GCode:
        break;
    case PlotProtocol::CAMM_GL1:
        write_payload("FS" + std::to_string(f) + ';');
        break;
    }
}

void PlotStreamEncoder::set_velocity(int v)
{
    switch (s_.protocol) {
    case PlotProtocol::HPGL:
        write_payload("VS" + std::to_string(v) + ';');
        break;
    case PlotProtocol::DMPL:
        write_payload("V" + std::to_string(v) + ' ');
        break;
    case PlotProtocol::GPGL:
        write_payload(std::string{'\x03', '!'} + std::to_string(v));
        break;
    case PlotProtocol::GCode:
        break;
    case PlotProtocol::CAMM_GL1:
        write_payload("VS" + std::to_string(v) + ';');
        break;
    }
}

void PlotStreamEncoder::set_pen(int p)
{
    switch (s_.protocol) {
    case PlotProtocol::HPGL:
        write_payload("SP" + std::to_string(p) + ';');
        break;
    case PlotProtocol::DMPL:
        write_payload("EC" + std::to_string(p) + ' ');
        break;
    case PlotProtocol::GPGL:
        break;
    case PlotProtocol::GCode:
        break;
    case PlotProtocol::CAMM_GL1:
        write_payload("SP" + std::to_string(p) + ';');
        break;
    }
}

void PlotStreamEncoder::finish()
{
    switch (s_.protocol) {
    case PlotProtocol::HPGL:
        write_payload("IN;");
        break;
    case PlotProtocol::DMPL:
        break;
    case PlotProtocol::GPGL:
        break;
    case PlotProtocol::GCode:
        if (s_.gcode.use_builtin) {
            if (s_.gcode.dialect == GCodeProtocolSettings::Dialect::Grbl) {
                if (s_.gcode.lift_mode == GCodeProtocolSettings::ZAxis) {
                    const int prec = s_.gcode.precision;
                    write_payload(
                        QStringLiteral("G0 Z%1; pen up\n")
                            .arg(s_.gcode.upper_z, 0, 'f', prec)
                            .toStdString());
                }
                write_payload("M5\n");
            } else {
                write_payload("G28; Return to home\n");
                write_payload("G98; Return to initial z\n");
            }
        }
        break;
    case PlotProtocol::CAMM_GL1:
        break;
    }
}

void append_polyline_plot(PlotStreamEncoder& enc, const std::vector<QPointF>& poly)
{
    if (poly.size() < 2)
        return;
    enc.move(poly.front().x(), poly.front().y(), 0.0, true);
    for (size_t k = 1; k < poly.size(); ++k)
        enc.move(poly[k].x(), poly[k].y(), 1.0, true);
}

std::string encode_pen_up_absolute_user_xy(double x, double y, const ProtocolSettings& ps)
{
    return encode_move_absolute_user_xy(x, y, 0.0, ps);
}

std::string encode_move_absolute_user_xy(double x, double y, double z, const ProtocolSettings& ps,
                                         bool previous_pen_up)
{
    std::string acc;
    PlotStreamEncoder enc([&](std::string chunk) { acc += std::move(chunk); }, ps);
    enc.set_initial_pen_up(previous_pen_up);
    enc.move(x, y, z, true);
    return acc;
}

std::string encode_set_origin_user_xy(double x, double y, const ProtocolSettings& ps)
{
    if (ps.protocol != PlotProtocol::GCode)
        return {};
    std::string acc;
    PlotStreamEncoder enc([&](std::string chunk) { acc += std::move(chunk); }, ps);
    const int prec = ps.gcode.precision;
    enc.send_command_block(
        QStringLiteral("G92 X%1 Y%2; set origin\n").arg(x, 0, 'f', prec).arg(y, 0, 'f', prec));
    return acc;
}

} // namespace inkcut
