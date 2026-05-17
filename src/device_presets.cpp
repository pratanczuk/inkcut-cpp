// SPDX-License-Identifier: GPL-3.0-or-later

#include "device_presets.hpp"

namespace inkcut {

namespace {

const QVector<DevicePreset> kPresets = []() {
    QVector<DevicePreset> v;
    auto add = [&](DevicePreset p) { v.push_back(std::move(p)); };
    add({QStringLiteral("grbl"), QStringLiteral("GRBLHAL"), QStringLiteral("CNC"), 300, 300,
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
    job.protocol.protocol = PlotProtocol::GCode;
    job.protocol.dmpl_mode = preset.dmpl_mode;
    job.device.swap_xy = preset.swap_xy;
    job.device.mirror_x = preset.mirror_x;
    job.device.mirror_y = preset.mirror_y;
    job.device.baud_rate = preset.default_baud;
    job.protocol.gcode.dialect = preset.gcode_dialect;
    job.protocol.gcode.lift_mode = preset.gcode_lift_mode;
    job.protocol.gcode.solenoid_pwm_up = preset.solenoid_pwm_up;
    job.protocol.gcode.solenoid_pwm_down = preset.solenoid_pwm_down;
    job.protocol.gcode.solenoid_pwm_max = preset.solenoid_pwm_max;
}

} // namespace inkcut
