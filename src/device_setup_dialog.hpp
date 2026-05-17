// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "job_model.hpp"

class QWidget;

namespace inkcut {

class DevicePluginLoader;

/// Dialog konfiguracji urządzenia (odpowiednik Device → Setup w upstream).
bool runDeviceSetupDialog(QWidget* parent, PlotJobSettings& job, DevicePluginLoader* plugins);

/// Wczytuje zapisany profil urządzenia (device_profiles_v1) do ustawień zadania.
void applyPersistedDeviceProfile(PlotJobSettings& job);

} // namespace inkcut
