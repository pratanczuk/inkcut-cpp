// SPDX-License-Identifier: GPL-3.0-or-later
/** Unified plot protocol encoder (HPGL, DMPL, GPGL, G-code, CAMM GL-1). */

#pragma once

#include <QPointF>
#include <functional>
#include <QString>
#include <vector>

namespace inkcut {

enum class PlotProtocol {
    HPGL,
    DMPL,
    GPGL,
    GCode,
    CAMM_GL1,
};

struct GCodeProtocolSettings {
    bool use_builtin = true;
    enum LiftMode { Implicit = 0, Custom = 1, ZAxis = 2 };
    enum class Dialect { Generic = 0, Grbl = 1 };
    LiftMode lift_mode = Implicit;
    Dialect dialect = Dialect::Generic;
    int precision = 3;
    double lower_z = 0;
    double upper_z = 1;
    QString lift_gcode;
    QString lower_gcode;
};

struct ProtocolSettings {
    PlotProtocol protocol = PlotProtocol::HPGL;
    bool hpgl_pad = false;
    /// DMPL mode per upstream (1,2,3,4,6)
    int dmpl_mode = 1;
    /// Scale used by HPGL/DMPL (user units → plotter units)
    double plot_scale = 1021.0 / 90.0;
    GCodeProtocolSettings gcode;
};

class PlotStreamEncoder {
public:
    PlotStreamEncoder(std::function<void(std::string)> sink, ProtocolSettings settings);

    void connection_made();
    void move(double x, double y, double z, bool absolute = true);
    void set_force(int f);
    void set_velocity(int v);
    void set_pen(int p);
    void finish();
    void connection_lost();

    /// Własne komendy (G-code lub HPGL/DMPL jako tekst, linie rozdzielone \\n).
    void send_command_block(const QString& commands);

private:
    void write_payload(std::string data);
    void send_gcode_block(const QString& commands);

    void move_hpgl(double x, double y, double z, bool absolute);
    void move_dmpl(double x, double y, double z, bool absolute);
    void move_gpgl(double x, double y, double z, bool absolute);
    void move_gcode(double x, double y, double z, bool absolute);
    void move_camm(double x, double y, double z, bool absolute);

    std::function<void(std::string)> sink_;
    ProtocolSettings s_;
    bool gcode_currently_up_ = false;
};

void append_polyline_plot(PlotStreamEncoder& enc, const std::vector<QPointF>& poly);

/// Pojedynczy absolutny ruch z piórem w górze (`z == 0` → PU/U/G0 wg protokołu), bez nagłówka połączenia.
/// Jednostki użytkownika jak `PlotStreamEncoder::move(..., absolute=true)`.
std::string encode_pen_up_absolute_user_xy(double x, double y, const ProtocolSettings& ps);

/// Absolutny ruch (z=0 pióro w górze, z≠0 pióro w dół) w jednostkach użytkownika.
std::string encode_move_absolute_user_xy(double x, double y, double z, const ProtocolSettings& ps);

/// Ustawienie bieżącej pozycji jako początek (G92 dla G-code, brak dla HPGL).
std::string encode_set_origin_user_xy(double x, double y, const ProtocolSettings& ps);

} // namespace inkcut
