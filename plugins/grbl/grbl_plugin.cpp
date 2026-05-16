// SPDX-License-Identifier: GPL-3.0-or-later

#include "device_plugin.hpp"
#include "device_presets.hpp"

#include <QObject>
#include <QSerialPortInfo>

namespace inkcut {

class GrblDevicePlugin final : public DevicePlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID InkcutDevicePluginIID FILE "grbl_plugin.json")
public:
    QString pluginId() const override { return QStringLiteral("grbl"); }

    QString displayName() const override { return QStringLiteral("GRBL (G-code / CNC)"); }

    PlotProtocol suggestedProtocol() const override { return PlotProtocol::GCode; }

    QStringList providedPresetIds() const override { return {QStringLiteral("grbl")}; }

    QVector<SerialDeviceDescriptor> probeSerialPorts() const override
    {
        QVector<SerialDeviceDescriptor> out;
        for (const QSerialPortInfo& info : QSerialPortInfo::availablePorts()) {
            const QString port = info.systemLocation();
            if (port.isEmpty())
                continue;
            QString desc = info.description();
            if (desc.isEmpty() && !info.manufacturer().isEmpty())
                desc = info.manufacturer();
            if (desc.trimmed().isEmpty())
                desc = port;
            out.push_back(SerialDeviceDescriptor{port, desc});
        }
        if (out.isEmpty())
            out.push_back({QStringLiteral("/dev/ttyUSB0"), QStringLiteral("USB serial (domyślny)")});
        return out;
    }

    void configureJobDefaults(PlotJobSettings& job) const override
    {
        DevicePreset preset;
        if (devicePresetById(QStringLiteral("grbl"), preset))
            applyPresetToJob(preset, job);

        job.protocol.protocol = PlotProtocol::GCode;
        job.protocol.plot_scale = 1.0;
        job.protocol.gcode.use_builtin = true;
        job.protocol.gcode.dialect = GCodeProtocolSettings::Dialect::Grbl;
        job.protocol.gcode.lift_mode = GCodeProtocolSettings::ZAxis;
        job.protocol.gcode.precision = 3;
        job.protocol.gcode.upper_z = 5.0;
        job.protocol.gcode.lower_z = 0.0;
        job.device.baud_rate = 115200;
        job.device.transport = PlotTransportKind::SerialPort;
        if (job.device.name.isEmpty())
            job.device.name = QStringLiteral("GRBL");
    }
};

} // namespace inkcut

#include "grbl_plugin.moc"
