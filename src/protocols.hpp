// SPDX-License-Identifier: GPL-3.0-or-later
/** Unified plot protocol encoder (G-code / GRBL). */

#pragma once

#include <QPointF>
#include <functional>
#include <QString>
#include <vector>

namespace inkcut {

enum class PlotProtocol {
    GCode,
};

struct GCodeProtocolSettings {
    bool use_builtin = true;
    enum LiftMode { Implicit = 0, Custom = 1, ZAxis = 2, SolenoidPwm = 3 };
    enum class Dialect { Generic = 0, Grbl = 1 };
    LiftMode lift_mode = Implicit;
    Dialect dialect = Dialect::Generic;
    int precision = 3;
    double lower_z = 0;
    double upper_z = 1;
    QString lift_gcode;
    QString lower_gcode;
    /// PWM solenoidu Z (np. GRBL pin spindle): wartość przy piórze w górze i w dole.
    int solenoid_pwm_up = 0;
    int solenoid_pwm_down = 700;
    int solenoid_pwm_max = 1000;
    /// Posuw G1 (mm/min). 0 = nie dodawaj `F<n>` — wtedy obowiązują `$110/$111` w GRBL
    /// lub wcześniej ustawione F. Wstawiane do każdego G1 dla pewności.
    int feed_mm_min = 0;
    /// Posuw G0 (mm/min). 0 = pomiń `F<n>` w G0.
    int feed_rapid_mm_min = 0;
};

struct ProtocolSettings {
    PlotProtocol protocol = PlotProtocol::GCode;
    double plot_scale = 1021.0 / 90.0;
    GCodeProtocolSettings gcode;
};

class PlotStreamEncoder {
public:
    PlotStreamEncoder(std::function<void(std::string)> sink, ProtocolSettings settings);

    void connection_made();
    void move(double x, double y, double z, bool absolute = true);
    /// Ustaw stan początkowy podnoszenia pióra (true = pióro w górze).
    /// Używane do pojedynczych komend z panelu Sterowanie — żeby pierwszy
    /// `pen down` faktycznie wyemitował komendę solenoidu/własnego G-code.
    void set_initial_pen_up(bool pen_up);
    void set_force(int f);
    void set_velocity(int v);
    void set_pen(int p);
    void finish();
    void connection_lost();

    /// Własne komendy G-code, linie rozdzielone \\n.
    void send_command_block(const QString& commands);

private:
    void write_payload(std::string data);
    void send_gcode_block(const QString& commands);

    void move_gcode(double x, double y, double z, bool absolute);

    std::function<void(std::string)> sink_;
    ProtocolSettings s_;
    bool gcode_currently_up_ = false;
};

void append_polyline_plot(PlotStreamEncoder& enc, const std::vector<QPointF>& poly);

/// Pojedynczy absolutny ruch z piórem w górze (`z == 0` -> G0), bez nagłówka połączenia.
/// Jednostki użytkownika jak `PlotStreamEncoder::move(..., absolute=true)`.
std::string encode_pen_up_absolute_user_xy(double x, double y, const ProtocolSettings& ps);

/// Absolutny ruch (z=0 pióro w górze, z≠0 pióro w dół) w jednostkach użytkownika.
/// `previous_pen_up` mówi enkoderowi w jakim stanie pióro było wcześniej —
/// dzięki temu pojedyncza komenda emituje również transition (M3/M5 itd.)
/// kiedy zmienia się stan pióra.
std::string encode_move_absolute_user_xy(double x, double y, double z, const ProtocolSettings& ps,
                                         bool previous_pen_up = true);

/// Ustawienie bieżącej pozycji jako początek (G92).
std::string encode_set_origin_user_xy(double x, double y, const ProtocolSettings& ps);

} // namespace inkcut
