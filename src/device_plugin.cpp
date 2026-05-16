// SPDX-License-Identifier: GPL-3.0-or-later

#include "device_plugin.hpp"

// Definicja klasy bazowej w bibliotece rdzenia — wymagane dla staticMetaObject
// przy dynamicznym ładowaniu modułów wtyczek (QObject / Q_PLUGIN_METADATA).

namespace inkcut {

DevicePlugin::DevicePlugin(QObject* parent) : QObject(parent) {}

QPainterPath DevicePlugin::transformPath(const QPainterPath& path) const
{
    return path;
}

} // namespace inkcut
