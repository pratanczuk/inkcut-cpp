// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "app_settings.hpp"
#include "job_model.hpp"

class QWidget;

namespace inkcut {

/// Dialog ustawień aplikacji (Job / Preview / System) jak w upstream Inkcut.
bool runSettingsDialog(QWidget* parent, AppSettings& app, PlotJobSettings& job);

} // namespace inkcut
