// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <memory>
#include <vector>

#include <QStringList>

class QPluginLoader;

namespace inkcut {

class DevicePlugin;

/// Loads `DevicePlugin` modules from standard locations (application plugins dir,
/// INKCUT_PLUGIN_PATH, `/usr/lib/inkcut/plugins`, local user data `…/inkcut/plugins`).
/// Keeps loaders alive for the lifetime of this object.
class DevicePluginLoader final {
public:
    DevicePluginLoader();
    ~DevicePluginLoader();

    DevicePluginLoader(const DevicePluginLoader&) = delete;
    DevicePluginLoader& operator=(const DevicePluginLoader&) = delete;

    /// Clears previous loaders and rescans the filesystem.
    void rescan(QStringList* errors);

    const std::vector<DevicePlugin*>& plugins() const { return instances_; }

    DevicePlugin* findById(const QString& plugin_id) const;

private:
    std::vector<std::unique_ptr<QPluginLoader>> loaders_;
    std::vector<DevicePlugin*> instances_;
};

} // namespace inkcut
