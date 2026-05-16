// SPDX-License-Identifier: GPL-3.0-or-later

#include "device_presets.hpp"

namespace inkcut {

namespace {

const QVector<DevicePreset> kPresets = []() {
    QVector<DevicePreset> v;
    auto add = [&](DevicePreset p) { v.push_back(std::move(p)); };
    add({QStringLiteral("generic"), QStringLiteral("Inkcut"), QStringLiteral("Generic"), 600, 400,
         PlotProtocol::HPGL, 1, false, false, false, 9600});
    add({QStringLiteral("roland-stika-15"), QStringLiteral("Roland"),
         QStringLiteral("Stika SV-15"), 381, 381, PlotProtocol::HPGL, 1, true, false, false, 9600});
    add({QStringLiteral("roland-pnc900"), QStringLiteral("Roland"), QStringLiteral("PNC-900"),
         304.8, 304.8, PlotProtocol::CAMM_GL1, 1, false, false, false, 9600});
    add({QStringLiteral("graphtec-ce6000"), QStringLiteral("Graphtec"),
         QStringLiteral("CE6000-60"), 762, 609.6, PlotProtocol::HPGL, 1, false, false, false, 9600});
    add({QStringLiteral("uscutter-mh"), QStringLiteral("US Cutter"), QStringLiteral("MH Series"),
         762, 609.6, PlotProtocol::HPGL, 1, false, false, false, 9600});
    add({QStringLiteral("silhouette-cameo"), QStringLiteral("Silhouette"),
         QStringLiteral("Cameo"), 304.8, 304.8, PlotProtocol::HPGL, 1, false, false, false, 9600});
    add({QStringLiteral("anagraph-ae70e"), QStringLiteral("ANAgraph"), QStringLiteral("AE-70e"),
         760, 12000, PlotProtocol::HPGL, 1, true, false, false, 9600});
    add({QStringLiteral("grbl"), QStringLiteral("GRBL"), QStringLiteral("CNC (serial)"), 300, 300,
         PlotProtocol::GCode, 1, false, false, false, 115200,
         GCodeProtocolSettings::Dialect::Grbl, GCodeProtocolSettings::ZAxis, 0, 700, 1000});
    add({QStringLiteral("grbl-solenoid"), QStringLiteral("DIY"),
         QStringLiteral("GRBL Solenoid PWM"), 300, 300, PlotProtocol::GCode, 1, false, false, false,
         115200, GCodeProtocolSettings::Dialect::Grbl, GCodeProtocolSettings::SolenoidPwm, 0, 700,
         1000});
    return v;
}();

} // namespace

const QVector<DevicePreset>& devicePresets()
{
    return kPresets;
}

bool devicePresetById(const QString& id, DevicePreset& out)
{
    for (const DevicePreset& p : kPresets) {
        if (p.id == id) {
            out = p;
            return true;
        }
    }
    return false;
}

void applyPresetToJob(const DevicePreset& preset, PlotJobSettings& job)
{
    job.device.preset_id = preset.id;
    job.device.manufacturer = preset.manufacturer;
    job.device.model_name = preset.model;
    if (job.device.name.isEmpty())
        job.device.name = QStringLiteral("%1 %2").arg(preset.manufacturer, preset.model);
    job.material.width = preset.material_width;
    job.material.height = preset.material_height;
    job.protocol.protocol = preset.default_protocol;
    job.protocol.dmpl_mode = preset.dmpl_mode;
    job.device.swap_xy = preset.swap_xy;
    job.device.mirror_x = preset.mirror_x;
    job.device.mirror_y = preset.mirror_y;
    job.device.baud_rate = preset.default_baud;
    if (preset.default_protocol == PlotProtocol::GCode) {
        job.protocol.gcode.dialect = preset.gcode_dialect;
        job.protocol.gcode.lift_mode = preset.gcode_lift_mode;
        job.protocol.gcode.solenoid_pwm_up = preset.solenoid_pwm_up;
        job.protocol.gcode.solenoid_pwm_down = preset.solenoid_pwm_down;
        job.protocol.gcode.solenoid_pwm_max = preset.solenoid_pwm_max;
    }
}

} // namespace inkcut
