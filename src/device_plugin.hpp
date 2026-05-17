// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "job_model.hpp"
#include "protocols.hpp"

#include <QObject>
#include <QPainterPath>
#include <QString>
#include <QStringList>
#include <QVector>

namespace inkcut {

struct SerialDeviceDescriptor {
    QString port_name;
    QString description;
};

/// Bazowa klasa wtyczek urządzeń (ładowane dynamicznie jako moduły `.so`).
/// IID musi być zgodny z Q_PLUGIN_METADATA w implementacji.
class DevicePlugin : public QObject {
    Q_OBJECT
public:
    explicit DevicePlugin(QObject* parent = nullptr);

    virtual QString pluginId() const = 0;
    virtual QString displayName() const = 0;

    virtual QVector<SerialDeviceDescriptor> probeSerialPorts() const { return { }; }

    /// Sugerowany protokół po wyborze wtyczki (opcjonalnie).
    virtual PlotProtocol suggestedProtocol() const { return PlotProtocol::GCode; }

    /// Transformacja ścieżki specyficzna dla urządzenia (swap_xy, lustro, skala).
    virtual QPainterPath transformPath(const QPainterPath& path) const;

    /// Lista identyfikatorów presetów dostarczanych przez wtyczkę.
    virtual QStringList providedPresetIds() const { return {}; }

    /// Ustawienia zadania po wyborze wtyczki (np. GRBL → G-code, 115200).
    virtual void configureJobDefaults(PlotJobSettings& job) const { Q_UNUSED(job); }
};

} // namespace inkcut

#define InkcutDevicePluginIID "inkcut.cpp.DevicePlugin/1.0"

Q_DECLARE_INTERFACE(inkcut::DevicePlugin, InkcutDevicePluginIID)
