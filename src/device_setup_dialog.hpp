// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "job_model.hpp"

class QWidget;

namespace inkcut {

class DevicePluginLoader;

/// Dialog konfiguracji urządzenia (odpowiednik Device → Setup w upstream).
bool runDeviceSetupDialog(QWidget* parent, PlotJobSettings& job, DevicePluginLoader* plugins);

} // namespace inkcut
