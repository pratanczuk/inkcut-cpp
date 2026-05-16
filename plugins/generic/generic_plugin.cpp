// SPDX-License-Identifier: GPL-3.0-or-later

#include "device_plugin.hpp"
#include "device_presets.hpp"

#include <QObject>

namespace inkcut {

class GenericDevicePlugin final : public DevicePlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID InkcutDevicePluginIID FILE "generic_plugin.json")
public:
    QString pluginId() const override { return QStringLiteral("generic-device"); }

    QString displayName() const override
    {
        return QStringLiteral("Generic HPGL (preset)");
    }

    QStringList providedPresetIds() const override
    {
        QStringList ids;
        for (const DevicePreset& p : devicePresets())
            ids.append(p.id);
        return ids;
    }

    QVector<SerialDeviceDescriptor> probeSerialPorts() const override
    {
        return {SerialDeviceDescriptor{QStringLiteral("/dev/ttyUSB0"),
                                      QStringLiteral("USB serial (domyślny)")}};
    }
};

} // namespace inkcut

#include "generic_plugin.moc"
