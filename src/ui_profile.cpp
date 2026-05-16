// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui_profile.hpp"

#include <QApplication>
#include <QDialog>
#include <QGuiApplication>
#include <QScreen>
#include <QWidget>

namespace inkcut {

QString uiProfileKey(UiProfile profile)
{
    switch (profile) {
    case UiProfile::Desktop:
        return QStringLiteral("desktop");
    case UiProfile::Tablet:
        return QStringLiteral("tablet");
    }
    return QStringLiteral("desktop");
}

UiProfile uiProfileFromKey(const QString& key)
{
    if (key == QLatin1String("tablet"))
        return UiProfile::Tablet;
    return UiProfile::Desktop;
}

UiProfileMetrics metricsFor(UiProfile profile)
{
    UiProfileMetrics m;
    switch (profile) {
    case UiProfile::Desktop:
        return m;
    case UiProfile::Tablet:
        m.default_window = QSize(1024, 600);
        m.maximize_on_show = true;
        m.plot_view_minimum = QSize(260, 180);
        m.left_dock_minimum_width = 260;
        m.filter_list_row_height = 34;
        m.filter_pass_button_px = 22;
        m.device_list_row_height = 48;
        m.control_button_min_px = 56;
        m.zoom_button_min_px = 60;
        m.control_icon_px = 30;
        m.control_grid_spacing = 8;
        m.bottom_dock_height = 210;
        m.settings_dialog = QSize(640, 440);
        m.settings_nav_max_width = 132;
        m.device_setup_dialog = QSize(820, 560);
        return m;
    }
    return m;
}

QString profileStyleSheet(UiProfile profile)
{
    if (profile != UiProfile::Tablet)
        return {};

    return QStringLiteral(
        "#control_tab QPushButton { min-height: 52px; min-width: 52px; }"
        "QPushButton#plot_zoom_btn { min-height: 56px; min-width: 56px; font-size: 22px; font-weight: bold; }"
        "QPushButton { min-height: 44px; min-width: 44px; padding: 6px 10px; font-size: 14px; }"
        "QCheckBox, QRadioButton { spacing: 8px; font-size: 14px; min-height: 32px; }"
        "QComboBox, QSpinBox, QDoubleSpinBox, QLineEdit { min-height: 36px; font-size: 14px; }"
        "QTabBar::tab { min-height: 36px; padding: 6px 12px; }"
        "QListWidget#device_profile_list::item { min-height: 48px; padding: 10px 8px; }"
        "QTableWidget { font-size: 13px; }"
        "QGroupBox { font-size: 14px; }"
        "QDialog QLineEdit, QDialog QComboBox, QDialog QSpinBox, QDialog QDoubleSpinBox {"
        "  min-height: 36px; }"
        "QDialog QFormLayout QLabel { min-height: 28px; padding-top: 4px; }"
        "QListWidget#filter_pass_list QCheckBox, QListWidget#filter_pass_list QLabel {"
        "  font-size: 14px; min-height: 0; }"
        "QSpinBox#filter_pass_spin { min-height: 0; max-height: 24px; min-width: 28px; max-width: 40px;"
        "  font-size: 14px; padding: 0 2px; }"
        "QPushButton#filter_pass_dec, QPushButton#filter_pass_inc {"
        "  min-width: 22px; max-width: 22px; min-height: 22px; max-height: 22px;"
        "  padding: 0; font-size: 14px; font-weight: bold; }"
        "QListWidget#filter_pass_list::item { margin-bottom: 6px; }");
}

QSize dialogSizeForProfile(const QSize& preferred, UiProfile profile)
{
    if (profile != UiProfile::Tablet)
        return preferred;

    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen)
        return preferred;

    const QRect avail = screen->availableGeometry();
    const int max_w = int(avail.width() * 0.92);
    const int max_h = int(avail.height() * 0.88);
    return QSize(qMin(preferred.width(), max_w), qMin(preferred.height(), max_h));
}

void applyDialogProfile(QDialog* dialog, const QSize& preferred, UiProfile profile)
{
    if (!dialog)
        return;
    const QSize size = dialogSizeForProfile(preferred, profile);
    dialog->resize(size);
    dialog->setMinimumSize(qMin(520, size.width()), qMin(420, size.height()));
    if (profile == UiProfile::Tablet)
        dialog->setStyleSheet(profileStyleSheet(profile));
}

} // namespace inkcut
