// SPDX-License-Identifier: GPL-3.0-or-later

#include "app_settings.hpp"

#include <QSettings>

namespace inkcut {

void loadAppSettings(AppSettings& out)
{
    QSettings settings(QStringLiteral("inkcut"), QStringLiteral("gui"));
    out.units = settings.value(QStringLiteral("units"), out.units).toString();
    out.optimizer_timeout =
        settings.value(QStringLiteral("optimizer_timeout"), out.optimizer_timeout).toDouble();
    out.flatten_step = settings.value(QStringLiteral("flatten_step"), out.flatten_step).toDouble();
    out.show_grid_x = settings.value(QStringLiteral("show_grid_x"), out.show_grid_x).toBool();
    out.show_grid_y = settings.value(QStringLiteral("show_grid_y"), out.show_grid_y).toBool();
    out.grid_alpha = settings.value(QStringLiteral("grid_alpha"), out.grid_alpha).toInt();
    out.dock_style = settings.value(QStringLiteral("dock_style"), out.dock_style).toString();
    out.language = settings.value(QStringLiteral("language"), out.language).toString();
    out.ui_profile = uiProfileFromKey(settings.value(QStringLiteral("ui_profile"), uiProfileKey(out.ui_profile))
                                             .toString());
    out.material_load_command =
        settings.value(QStringLiteral("material_load_command"), out.material_load_command).toString();
    out.material_unload_command =
        settings.value(QStringLiteral("material_unload_command"), out.material_unload_command)
            .toString();
}

void saveAppSettings(const AppSettings& s)
{
    QSettings settings(QStringLiteral("inkcut"), QStringLiteral("gui"));
    settings.setValue(QStringLiteral("units"), s.units);
    settings.setValue(QStringLiteral("optimizer_timeout"), s.optimizer_timeout);
    settings.setValue(QStringLiteral("flatten_step"), s.flatten_step);
    settings.setValue(QStringLiteral("show_grid_x"), s.show_grid_x);
    settings.setValue(QStringLiteral("show_grid_y"), s.show_grid_y);
    settings.setValue(QStringLiteral("grid_alpha"), s.grid_alpha);
    settings.setValue(QStringLiteral("dock_style"), s.dock_style);
    settings.setValue(QStringLiteral("language"), s.language);
    settings.setValue(QStringLiteral("ui_profile"), uiProfileKey(s.ui_profile));
    settings.setValue(QStringLiteral("material_load_command"), s.material_load_command);
    settings.setValue(QStringLiteral("material_unload_command"), s.material_unload_command);
}

} // namespace inkcut
