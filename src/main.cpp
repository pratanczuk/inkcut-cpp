// SPDX-License-Identifier: GPL-3.0-or-later

#include "dxf_document.hpp"
#include "job_export.hpp"
#include "job_pipeline.hpp"
#include "protocols.hpp"
#include "serial_sender.hpp"
#include "svg_document.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QPainterPath>
#include <QRectF>
#include <QSerialPort>
#include <QStringList>
#include <QtGlobal>

#include <cstring>
#include <iostream>
#include <string>

namespace {

using inkcut::OrderStrategy;
using inkcut::PlotJobSettings;
using inkcut::PlotProtocol;

void print_usage()
{
    std::cout
        << "inkcut-cpp — port C++ Inkcut (Linux): protokoły, filtry, kolejność cięcia, SVG/DXF\n\n"
        << "Użycie:\n"
        << "  inkcut-cpp send --port DEVICE [--baud N] [--pad] [--dry-run] [--protocol …] FILE\n"
        << "      Pliki .svg / .dxf są zamieniane na program plotera (jak svg-convert).\n"
        << "  inkcut-cpp rect ... [--protocol gcode]\n"
        << "  inkcut-cpp svg-convert INPUT.svg [opcje] [--out FILE|-]\n"
        << "  inkcut-cpp dxf-convert INPUT.dxf [opcje] [--out FILE|-]\n"
        << "  inkcut-cpp job-export INPUT.svg|dxf [opcje] [--out FILE|-]\n"
        << "      Zapis metadanych zadania (ustawienia + bbox) jako JSON.\n"
        << "  inkcut-cpp job-run JOB.json [--port DEVICE] [--baud N] [--pad] [--dry-run]\n"
        << "      Wczytuje inkcut-job JSON, odtwarza geometrię ze ścieżki źródłowej i wysyła jak send.\n\n"
        << "svg-convert / dxf-convert / job-export / send (wektor):\n"
        << "  --protocol gcode\n"
        << "  --step S               próbkowanie krzywych (jednostki użytkownika)\n"
        << "  --velocity VS\n"
        << "  --order normal|reversed|min-x|max-x|min-y|max-y|shortest|hilbert|zcurve\n"
        << "  --repeat-steps N [--repeat-gap G]\n"
        << "  --min-jump A [--min-path B] [--min-edge C] [--min-shift D]\n"
        << "  --blade-offset O [--blade-cutoff deg] [--blade-quality Q]\n"
        << "  --overcut V [--closed-eps E]\n"
        << "  --pad                  HPGL: dodaj LF po każdym write\n";
}

bool streq(const char* a, const char* b)
{
    return std::strcmp(a, b) == 0;
}

bool write_string_output(const QString& path_or_dash, const std::string& data)
{
    if (path_or_dash == QLatin1String("-")) {
        std::cout.write(data.data(), static_cast<std::streamsize>(data.size()));
        std::cout.flush();
        return true;
    }
    QFile out(path_or_dash);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        std::cerr << "Nie można zapisać: " << qPrintable(path_or_dash) << "\n";
        return false;
    }
    return out.write(data.data(), static_cast<qint64>(data.size())) == static_cast<qint64>(data.size());
}

bool write_utf8_output(const QString& path_or_dash, const QString& utf16text)
{
    const QByteArray utf8 = utf16text.toUtf8();
    return write_string_output(path_or_dash, std::string(utf8.constData(), utf8.size()));
}

bool consume_plot_job_setting(const QStringList& args, int& i, PlotJobSettings& job, QString* err)
{
    const QString& tok = args.at(i);

    auto bad = [&](const char* msg) {
        if (err)
            *err = QString::fromUtf8(msg);
        return false;
    };

    if (tok == QLatin1String("--step")) {
        if (i + 1 >= args.size())
            return bad("send: brak wartości dla --step");
        job.flatten_step = args.at(++i).toDouble();
        ++i;
        return true;
    }
    if (tok == QLatin1String("--velocity")) {
        if (i + 1 >= args.size())
            return bad("send: brak wartości dla --velocity");
        job.velocity = args.at(++i).toInt();
        ++i;
        return true;
    }
    if (tok == QLatin1String("--protocol")) {
        if (i + 1 >= args.size())
            return bad("send: brak wartości dla --protocol");
        job.protocol.protocol = inkcut::plotProtocolFromCli(args.at(++i));
        ++i;
        return true;
    }
    if (tok == QLatin1String("--pad")) {
        ++i;
        return true;
    }
    if (tok == QLatin1String("--order")) {
        if (i + 1 >= args.size())
            return bad("send: brak wartości dla --order");
        OrderStrategy os = OrderStrategy::Normal;
        if (!inkcut::orderStrategyFromCli(QStringView(args.at(++i)), os))
            return bad("send: nieznana wartość --order");
        job.order = os;
        ++i;
        return true;
    }
    if (tok == QLatin1String("--repeat-steps")) {
        if (i + 1 >= args.size())
            return bad("send: brak wartości dla --repeat-steps");
        job.repeat.steps = args.at(++i).toInt();
        ++i;
        return true;
    }
    if (tok == QLatin1String("--repeat-gap")) {
        if (i + 1 >= args.size())
            return bad("send: brak wartości dla --repeat-gap");
        job.repeat.closed_loop_distance = args.at(++i).toDouble();
        ++i;
        return true;
    }
    if (tok == QLatin1String("--min-jump")) {
        if (i + 1 >= args.size())
            return bad("send: brak wartości dla --min-jump");
        job.min_line.min_jump = args.at(++i).toDouble();
        ++i;
        return true;
    }
    if (tok == QLatin1String("--min-path")) {
        if (i + 1 >= args.size())
            return bad("send: brak wartości dla --min-path");
        job.min_line.min_path = args.at(++i).toDouble();
        ++i;
        return true;
    }
    if (tok == QLatin1String("--min-edge")) {
        if (i + 1 >= args.size())
            return bad("send: brak wartości dla --min-edge");
        job.min_line.min_edge = args.at(++i).toDouble();
        ++i;
        return true;
    }
    if (tok == QLatin1String("--min-shift")) {
        if (i + 1 >= args.size())
            return bad("send: brak wartości dla --min-shift");
        job.min_line.min_shift = args.at(++i).toDouble();
        ++i;
        return true;
    }
    if (tok == QLatin1String("--blade-offset")) {
        if (i + 1 >= args.size())
            return bad("send: brak wartości dla --blade-offset");
        job.blade.offset = args.at(++i).toDouble();
        ++i;
        return true;
    }
    if (tok == QLatin1String("--blade-cutoff")) {
        if (i + 1 >= args.size())
            return bad("send: brak wartości dla --blade-cutoff");
        job.blade.cutoff_deg = args.at(++i).toDouble();
        ++i;
        return true;
    }
    if (tok == QLatin1String("--blade-quality")) {
        if (i + 1 >= args.size())
            return bad("send: brak wartości dla --blade-quality");
        job.blade.quality_factor = args.at(++i).toDouble();
        ++i;
        return true;
    }
    if (tok == QLatin1String("--overcut")) {
        if (i + 1 >= args.size())
            return bad("send: brak wartości dla --overcut");
        job.overcut = args.at(++i).toDouble();
        ++i;
        return true;
    }
    if (tok == QLatin1String("--closed-eps")) {
        if (i + 1 >= args.size())
            return bad("send: brak wartości dla --closed-eps");
        job.closed_poly_eps = args.at(++i).toDouble();
        ++i;
        return true;
    }

    Q_UNUSED(err);
    return false;
}

bool parse_vector_convert_cli(const QStringList& args, QString& in_path, QString& out_path,
                              PlotJobSettings& job, QString* err)
{
    int i = 2;
    out_path = QLatin1String("-");

    while (i < args.size()) {
        const QString& tok = args.at(i);
        if (tok == QLatin1String("--out")) {
            if (i + 1 >= args.size()) {
                if (err)
                    *err = QStringLiteral("Brak wartości dla --out");
                return false;
            }
            out_path = args.at(++i);
            ++i;
            continue;
        }
        if (consume_plot_job_setting(args, i, job, err))
            continue;
        if (!tok.startsWith(QLatin1Char('-'))) {
            if (!in_path.isEmpty()) {
                if (err)
                    *err = QStringLiteral("Podano więcej niż jeden plik wejściowy.");
                return false;
            }
            in_path = tok;
            ++i;
            continue;
        }
        if (err)
            *err = QStringLiteral("Nieznana opcja: %1").arg(tok);
        return false;
    }

    if (in_path.isEmpty()) {
        if (err)
            *err = QStringLiteral("Brak pliku wejściowego.");
        return false;
    }
    return true;
}

struct SendCliOpts {
    QString port;
    qint32 baud = 9600;
    bool pad = false;
    bool dry = false;
    QString file_path;
    PlotJobSettings job;
};

bool parse_send_cli(const QStringList& args, SendCliOpts& o, QString* err)
{
    int i = 2;

    while (i < args.size()) {
        const QString& tok = args.at(i);
        if (tok == QLatin1String("--port")) {
            if (i + 1 >= args.size()) {
                if (err)
                    *err = QStringLiteral("Brak wartości dla --port");
                return false;
            }
            o.port = args.at(++i);
            ++i;
            continue;
        }
        if (tok == QLatin1String("--baud")) {
            if (i + 1 >= args.size()) {
                if (err)
                    *err = QStringLiteral("Brak wartości dla --baud");
                return false;
            }
            o.baud = args.at(++i).toInt();
            ++i;
            continue;
        }
        if (tok == QLatin1String("--pad")) {
            o.pad = true;
            ++i;
            continue;
        }
        if (tok == QLatin1String("--dry-run")) {
            o.dry = true;
            ++i;
            continue;
        }
        if (consume_plot_job_setting(args, i, o.job, err))
            continue;
        if (!tok.startsWith(QLatin1Char('-'))) {
            if (!o.file_path.isEmpty()) {
                if (err)
                    *err = QStringLiteral("Podano więcej niż jeden plik.");
                return false;
            }
            o.file_path = tok;
            ++i;
            continue;
        }
        if (err)
            *err = QStringLiteral("Nieznana opcja: %1").arg(tok);
        return false;
    }

    if (o.port.isEmpty() || o.file_path.isEmpty()) {
        if (err)
            *err = QStringLiteral("send: wymagane --port oraz ścieżka pliku.");
        return false;
    }
    return true;
}

bool load_design_bytes(const QString& path, const QByteArray& raw, QPainterPath& combined,
                       QString* err)
{
    if (path.endsWith(QLatin1String(".dxf"), Qt::CaseInsensitive))
        return inkcut::loadDxfPainterPathFromBytes(raw, combined, err);
    if (path.endsWith(QLatin1String(".svg"), Qt::CaseInsensitive))
        return inkcut::loadSvgPainterPath(QString::fromUtf8(raw), combined, err,
                                          QFileInfo(path).absolutePath(), nullptr);
    return false;
}

bool is_vector_design_path(const QString& path)
{
    return path.endsWith(QLatin1String(".svg"), Qt::CaseInsensitive)
        || path.endsWith(QLatin1String(".dxf"), Qt::CaseInsensitive);
}

int cmd_send(QCoreApplication& app, const QStringList& args)
{
    SendCliOpts o;
    QString perr;
    if (!parse_send_cli(args, o, &perr)) {
        std::cerr << qPrintable(perr) << "\n";
        print_usage();
        return 2;
    }

    QFile f(o.file_path);
    if (!f.open(QIODevice::ReadOnly)) {
        std::cerr << "Nie można otworzyć pliku: " << qPrintable(o.file_path) << "\n";
        return 1;
    }
    const QByteArray raw = f.readAll();

    QByteArray payload;

    if (is_vector_design_path(o.file_path)) {
        QPainterPath combined;
        QString le;
        if (!load_design_bytes(o.file_path, raw, combined, &le)) {
            std::cerr << qPrintable(le) << "\n";
            return 1;
        }
        const std::string prog = inkcut::buildPlotProgram(combined, o.job);
        payload = QByteArray::fromStdString(prog);
    } else {
        payload = raw;
    }

    if (o.dry) {
        std::cout.write(payload.constData(), payload.size());
        if (o.pad && !payload.endsWith('\n'))
            std::cout.put('\n');
        std::cout.flush();
        Q_UNUSED(app);
        return 0;
    }

    QSerialPort serial;
    inkcut::SerialOpenOptions opt;
    opt.port_name = o.port;
    opt.baud_rate = o.baud;
    if (!inkcut::open_serial(serial, opt)) {
        std::cerr << "Otwarcie portu nie powiodło się: " << qPrintable(o.port) << "\n";
        return 1;
    }

    if (o.pad && !payload.endsWith('\n'))
        payload.append('\n');

    if (!inkcut::write_all(serial, payload)) {
        std::cerr << "Zapis na port nie powiódł się.\n";
        return 1;
    }

    Q_UNUSED(app);
    return 0;
}

int cmd_rect(const QStringList& args)
{
    double w = -1;
    double h = -1;
    double x = 0;
    double y = 0;
    int velocity = -1;
    QString out_path = QLatin1String("-");
    PlotJobSettings job;
    job.protocol.protocol = PlotProtocol::GCode;

    for (int i = 2; i < args.size(); ++i) {
        const QByteArray a = args.at(i).toUtf8();
        const char* s = a.constData();
        if (streq(s, "--width") && i + 1 < args.size()) {
            w = args.at(++i).toDouble();
        } else if (streq(s, "--height") && i + 1 < args.size()) {
            h = args.at(++i).toDouble();
        } else if (streq(s, "--x") && i + 1 < args.size()) {
            x = args.at(++i).toDouble();
        } else if (streq(s, "--y") && i + 1 < args.size()) {
            y = args.at(++i).toDouble();
        } else if (streq(s, "--velocity") && i + 1 < args.size()) {
            velocity = args.at(++i).toInt();
        } else if (streq(s, "--out") && i + 1 < args.size()) {
            out_path = args.at(++i);
        } else if (streq(s, "--protocol") && i + 1 < args.size()) {
            job.protocol.protocol = inkcut::plotProtocolFromCli(args.at(++i));
        } else if (streq(s, "--pad")) {
        } else {
            std::cerr << "Nieznana opcja: " << s << "\n";
            return 2;
        }
    }

    if (w <= 0 || h <= 0) {
        print_usage();
        return 2;
    }

    QPainterPath rect_path;
    rect_path.moveTo(x, y);
    rect_path.lineTo(x + w, y);
    rect_path.lineTo(x + w, y + h);
    rect_path.lineTo(x, y + h);
    rect_path.closeSubpath();

    job.flatten_step = 0.5;
    job.velocity = velocity;

    std::string acc = inkcut::buildPlotProgram(rect_path, job);

    if (!write_string_output(out_path, acc))
        return 1;
    return 0;
}

int cmd_svg_convert(const QStringList& args)
{
    QString in_path;
    QString out_path;
    PlotJobSettings job;

    QString err;
    if (!parse_vector_convert_cli(args, in_path, out_path, job, &err)) {
        std::cerr << qPrintable(err) << "\n";
        print_usage();
        return 2;
    }

    QFile f(in_path);
    if (!f.open(QIODevice::ReadOnly)) {
        std::cerr << "Nie można otworzyć SVG: " << qPrintable(in_path) << "\n";
        return 1;
    }
    const QByteArray raw = f.readAll();

    QPainterPath combined;
    QString le;
    if (!inkcut::loadSvgPainterPath(QString::fromUtf8(raw), combined, &le,
                                    QFileInfo(in_path).absolutePath(), nullptr)) {
        std::cerr << qPrintable(le) << "\n";
        return 1;
    }

    const std::string acc = inkcut::buildPlotProgram(combined, job);

    if (!write_string_output(out_path, acc))
        return 1;
    return 0;
}

int cmd_dxf_convert(const QStringList& args)
{
    QString in_path;
    QString out_path;
    PlotJobSettings job;

    QString err;
    if (!parse_vector_convert_cli(args, in_path, out_path, job, &err)) {
        std::cerr << qPrintable(err) << "\n";
        print_usage();
        return 2;
    }

    QFile f(in_path);
    if (!f.open(QIODevice::ReadOnly)) {
        std::cerr << "Nie można otworzyć DXF: " << qPrintable(in_path) << "\n";
        return 1;
    }
    const QByteArray raw = f.readAll();

    QPainterPath combined;
    QString le;
    if (!inkcut::loadDxfPainterPathFromBytes(raw, combined, &le)) {
        std::cerr << qPrintable(le) << "\n";
        return 1;
    }

    const std::string acc = inkcut::buildPlotProgram(combined, job);

    if (!write_string_output(out_path, acc))
        return 1;
    return 0;
}

int cmd_job_export(const QStringList& args)
{
    QString in_path;
    QString out_path;
    PlotJobSettings job;

    QString err;
    if (!parse_vector_convert_cli(args, in_path, out_path, job, &err)) {
        std::cerr << qPrintable(err) << "\n";
        print_usage();
        return 2;
    }

    const QString lower = in_path.toLower();
    if (!lower.endsWith(QLatin1String(".svg")) && !lower.endsWith(QLatin1String(".dxf"))) {
        std::cerr << "job-export: obsługiwane są tylko .svg i .dxf.\n";
        return 2;
    }

    QFile f(in_path);
    if (!f.open(QIODevice::ReadOnly)) {
        std::cerr << "Nie można otworzyć pliku: " << qPrintable(in_path) << "\n";
        return 1;
    }
    const QByteArray raw = f.readAll();

    QPainterPath combined;
    QString le;
    if (!load_design_bytes(in_path, raw, combined, &le)) {
        std::cerr << qPrintable(le) << "\n";
        return 1;
    }

    const QString abs = QFileInfo(in_path).absoluteFilePath();
    const QString kind =
        lower.endsWith(QLatin1String(".dxf")) ? QStringLiteral("dxf") : QStringLiteral("svg");
    const QString json =
        inkcut::exportJobDocumentToJson(abs, kind, combined.boundingRect(), job);

    if (!write_utf8_output(out_path, json))
        return 1;
    return 0;
}

struct JobRunOpts {
    QString port;
    qint32 baud = 9600;
    bool pad = false;
    bool dry = false;
    QString json_path;
};

bool parse_job_run_cli(const QStringList& args, JobRunOpts& o, QString* err)
{
    int i = 2;

    while (i < args.size()) {
        const QString& tok = args.at(i);
        if (tok == QLatin1String("--port")) {
            if (i + 1 >= args.size()) {
                if (err)
                    *err = QStringLiteral("Brak wartości dla --port");
                return false;
            }
            o.port = args.at(++i);
            ++i;
            continue;
        }
        if (tok == QLatin1String("--baud")) {
            if (i + 1 >= args.size()) {
                if (err)
                    *err = QStringLiteral("Brak wartości dla --baud");
                return false;
            }
            o.baud = args.at(++i).toInt();
            ++i;
            continue;
        }
        if (tok == QLatin1String("--pad")) {
            o.pad = true;
            ++i;
            continue;
        }
        if (tok == QLatin1String("--dry-run")) {
            o.dry = true;
            ++i;
            continue;
        }
        if (!tok.startsWith(QLatin1Char('-'))) {
            if (!o.json_path.isEmpty()) {
                if (err)
                    *err = QStringLiteral("Podano więcej niż jeden plik JSON.");
                return false;
            }
            o.json_path = tok;
            ++i;
            continue;
        }
        if (err)
            *err = QStringLiteral("Nieznana opcja: %1").arg(tok);
        return false;
    }

    if (o.port.isEmpty() || o.json_path.isEmpty()) {
        if (err)
            *err = QStringLiteral("job-run: wymagane JOB.json oraz --port.");
        return false;
    }
    return true;
}

int cmd_job_run(QCoreApplication& app, const QStringList& args)
{
    JobRunOpts o;
    QString perr;
    if (!parse_job_run_cli(args, o, &perr)) {
        std::cerr << qPrintable(perr) << "\n";
        print_usage();
        return 2;
    }

    QFile jf(o.json_path);
    if (!jf.open(QIODevice::ReadOnly)) {
        std::cerr << "Nie można otworzyć JSON: " << qPrintable(o.json_path) << "\n";
        return 1;
    }
    const QByteArray jraw = jf.readAll();

    inkcut::PlotJobSettings job;
    QString source_path;
    QString kind;
    QRectF bounds;
    QString ie;
    if (!inkcut::importJobDocumentJson(jraw, job, &source_path, &kind, &bounds, &ie)) {
        std::cerr << qPrintable(ie) << "\n";
        return 1;
    }
    Q_UNUSED(kind);
    Q_UNUSED(bounds);

    QFile df(source_path);
    if (!df.open(QIODevice::ReadOnly)) {
        std::cerr << "Nie można otworzyć źródła z JSON: " << qPrintable(source_path) << "\n";
        return 1;
    }
    const QByteArray draw = df.readAll();

    QPainterPath combined;
    QString le;
    if (!load_design_bytes(source_path, draw, combined, &le)) {
        std::cerr << qPrintable(le) << "\n";
        return 1;
    }

    const std::string prog = inkcut::buildPlotProgram(combined, job);
    QByteArray payload = QByteArray::fromStdString(prog);

    if (o.dry) {
        std::cout.write(payload.constData(), payload.size());
        if (o.pad && !payload.endsWith('\n'))
            std::cout.put('\n');
        std::cout.flush();
        Q_UNUSED(app);
        return 0;
    }

    QSerialPort serial;
    inkcut::SerialOpenOptions opt;
    opt.port_name = o.port;
    opt.baud_rate = o.baud;
    if (!inkcut::open_serial(serial, opt)) {
        std::cerr << "Otwarcie portu nie powiodło się: " << qPrintable(o.port) << "\n";
        return 1;
    }

    if (o.pad && !payload.endsWith('\n'))
        payload.append('\n');

    if (!inkcut::write_all(serial, payload)) {
        std::cerr << "Zapis na port nie powiódł się.\n";
        return 1;
    }

    Q_UNUSED(app);
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("inkcut-cpp"));

    const QStringList args = app.arguments();
    if (args.size() < 2) {
        print_usage();
        return 2;
    }

    const QString cmd = args.at(1);
    if (cmd == QLatin1String("-h") || cmd == QLatin1String("--help")) {
        print_usage();
        return 0;
    }
    if (cmd == QLatin1String("send"))
        return cmd_send(app, args);
    if (cmd == QLatin1String("rect"))
        return cmd_rect(args);
    if (cmd == QLatin1String("svg-convert"))
        return cmd_svg_convert(args);
    if (cmd == QLatin1String("dxf-convert"))
        return cmd_dxf_convert(args);
    if (cmd == QLatin1String("job-export"))
        return cmd_job_export(args);
    if (cmd == QLatin1String("job-run"))
        return cmd_job_run(app, args);

    print_usage();
    return 2;
}
