// SPDX-License-Identifier: GPL-3.0-or-later

#include "device_plugin.hpp"

#include <QObject>

namespace inkcut {

class NoopDevicePlugin final : public DevicePlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID InkcutDevicePluginIID FILE "noop_plugin.json")
public:
    QString pluginId() const override { return QStringLiteral("noop"); }

    QString displayName() const override
    {
        return QStringLiteral("No-op (test wtyczki)");
    }

    QStringList providedPresetIds() const override
    {
        return {QStringLiteral("generic")};
    }
};

} // namespace inkcut

#include "noop_plugin.moc"
