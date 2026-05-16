// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ui_profile.hpp"

#include <QString>

namespace inkcut {

struct AppSettings {
    QString units = QStringLiteral("mm");
    double optimizer_timeout = 10.0;
    double flatten_step = 1.0;

    bool show_grid_x = true;
    bool show_grid_y = true;
    int grid_alpha = 80;

    QString dock_style = QStringLiteral("system");
    QString language = QStringLiteral("en");
    UiProfile ui_profile = UiProfile::Desktop;

    /// Surowe komendy wysyłane na port przy sterowaniu ręcznym (np. HPGL `PG;` lub G-code).
    QString material_load_command;
    QString material_unload_command;
};

void loadAppSettings(AppSettings& out);
void saveAppSettings(const AppSettings& settings);

} // namespace inkcut
