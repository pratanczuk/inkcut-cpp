// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QSize>
#include <QString>

class QDialog;
class QWidget;

namespace inkcut {

enum class UiProfile { Desktop, Tablet };

QString uiProfileKey(UiProfile profile);
UiProfile uiProfileFromKey(const QString& key);

struct UiProfileMetrics {
    QSize default_window{1200, 800};
    bool maximize_on_show = false;
    QSize plot_view_minimum{540, 320};
    int left_dock_minimum_width = 200;
    int filter_list_row_height = 30;
    int filter_pass_button_px = 20;
    int device_list_row_height = 32;
    int control_button_min_px = 40;
    int zoom_button_min_px = 44;
    int control_icon_px = 22;
    int control_grid_spacing = 4;
    int bottom_dock_height = 220;
    QSize settings_dialog{720, 480};
    int settings_nav_max_width = 180;
    QSize device_setup_dialog{720, 420};
};

UiProfileMetrics metricsFor(UiProfile profile);
QString profileStyleSheet(UiProfile profile);

/// Rozmiar dialogu dopasowany do ekranu (tablet — nie większy niż ~92% dostępnej powierzchni).
QSize dialogSizeForProfile(const QSize& preferred, UiProfile profile);

void applyDialogProfile(QDialog* dialog, const QSize& preferred, UiProfile profile);

} // namespace inkcut
