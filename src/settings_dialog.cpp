// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings_dialog.hpp"

#include "i18n.hpp"
#include "ui_profile.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace inkcut {

namespace {

QWidget* makeJobPage(QWidget* parent, AppSettings& app)
{
    auto* page = new QWidget(parent);
    auto* form = new QFormLayout(page);

    auto* units = new QComboBox(page);
    units->addItem(QStringLiteral("mm"), QStringLiteral("mm"));
    units->addItem(QStringLiteral("in"), QStringLiteral("in"));
    units->addItem(QStringLiteral("cm"), QStringLiteral("cm"));
    units->addItem(QStringLiteral("m"), QStringLiteral("m"));
    units->addItem(QStringLiteral("px"), QStringLiteral("px"));
    const int ui = units->findData(app.units);
    units->setCurrentIndex(ui >= 0 ? ui : 0);

    auto* timeout = new QDoubleSpinBox(page);
    timeout->setRange(0.1, 3600);
    timeout->setDecimals(2);
    timeout->setSuffix(QStringLiteral(" s"));
    timeout->setValue(app.optimizer_timeout);

    auto* flatten = new QDoubleSpinBox(page);
    flatten->setRange(0.05, 500);
    flatten->setDecimals(3);
    flatten->setValue(app.flatten_step);

    form->addRow(trInk("Domyślne jednostki"), units);
    form->addRow(QStringLiteral("Limit optymalizatora"), timeout);
    form->addRow(trInk("Krok próbkowania"), flatten);

    QObject::connect(units, &QComboBox::currentIndexChanged, page, [units, &app]() {
        app.units = units->currentData().toString();
    });
    QObject::connect(timeout, qOverload<double>(&QDoubleSpinBox::valueChanged), page,
                     [&app](double v) { app.optimizer_timeout = v; });
    QObject::connect(flatten, qOverload<double>(&QDoubleSpinBox::valueChanged), page,
                     [&app](double v) { app.flatten_step = v; });

    return page;
}

QWidget* makePreviewPage(QWidget* parent, AppSettings& app)
{
    auto* page = new QWidget(parent);
    auto* form = new QFormLayout(page);

    auto* grid_x = new QCheckBox(QStringLiteral("X"), page);
    grid_x->setChecked(app.show_grid_x);
    auto* grid_y = new QCheckBox(QStringLiteral("Y"), page);
    grid_y->setChecked(app.show_grid_y);
    auto* alpha = new QSpinBox(page);
    alpha->setRange(0, 255);
    alpha->setValue(app.grid_alpha);

    auto* grid_row = new QWidget(page);
    auto* grid_h = new QHBoxLayout(grid_row);
    grid_h->setContentsMargins(0, 0, 0, 0);
    grid_h->addWidget(grid_x);
    grid_h->addWidget(grid_y);
    grid_h->addStretch(1);

    form->addRow(QStringLiteral("Siatka"), grid_row);
    form->addRow(trInk("Przezroczystość"), alpha);

    QObject::connect(grid_x, &QCheckBox::toggled, page, [&app](bool on) { app.show_grid_x = on; });
    QObject::connect(grid_y, &QCheckBox::toggled, page, [&app](bool on) { app.show_grid_y = on; });
    QObject::connect(alpha, qOverload<int>(&QSpinBox::valueChanged), page,
                     [&app](int v) { app.grid_alpha = v; });

    return page;
}

QWidget* makeSystemPage(QWidget* parent, AppSettings& app)
{
    auto* page = new QWidget(parent);
    auto* form = new QFormLayout(page);

    auto* theme = new QComboBox(page);
    theme->addItem(QStringLiteral("System"), QStringLiteral("system"));
    theme->addItem(QStringLiteral("Dark"), QStringLiteral("dark"));
    theme->addItem(QStringLiteral("Light"), QStringLiteral("light"));
    const int ti = theme->findData(app.dock_style);
    theme->setCurrentIndex(ti >= 0 ? ti : 0);

    auto* language = new QComboBox(page);
    language->addItem(QStringLiteral("English"), QStringLiteral("en"));
    language->addItem(QStringLiteral("Polski"), QStringLiteral("pl"));
    const int li = language->findData(app.language);
    language->setCurrentIndex(li >= 0 ? li : 0);

    auto* ui_profile = new QComboBox(page);
    ui_profile->addItem(QStringLiteral("Desktop"), uiProfileKey(UiProfile::Desktop));
    ui_profile->addItem(QStringLiteral("Tablet (7″+)"), uiProfileKey(UiProfile::Tablet));
    const int pi = ui_profile->findData(uiProfileKey(app.ui_profile));
    ui_profile->setCurrentIndex(pi >= 0 ? pi : 0);

    auto* hint = new QLabel(
        trInk("Motyw i język: restart aplikacji. Profil interfejsu stosuje się od razu po OK."),
        page);
    hint->setWordWrap(true);

    form->addRow(QStringLiteral("Motyw"), theme);
    form->addRow(trInk("Język"), language);
    form->addRow(QStringLiteral("Profil interfejsu"), ui_profile);
    form->addRow(hint);

    QObject::connect(theme, &QComboBox::currentIndexChanged, page, [theme, &app]() {
        app.dock_style = theme->currentData().toString();
    });
    QObject::connect(language, &QComboBox::currentIndexChanged, page, [language, &app]() {
        app.language = language->currentData().toString();
    });
    QObject::connect(ui_profile, &QComboBox::currentIndexChanged, page, [ui_profile, &app]() {
        app.ui_profile = uiProfileFromKey(ui_profile->currentData().toString());
    });

    return page;
}

QWidget* makeControlPage(QWidget* parent, AppSettings& app)
{
    auto* page = new QWidget(parent);
    auto* form = new QFormLayout(page);

    auto* load_cmd = new QLineEdit(page);
    load_cmd->setPlaceholderText(QStringLiteral("np. PG; lub G-code podawania"));
    load_cmd->setText(app.material_load_command);

    auto* unload_cmd = new QLineEdit(page);
    unload_cmd->setPlaceholderText(QStringLiteral("np. PG; lub G-code cofania"));
    unload_cmd->setText(app.material_unload_command);

    auto* hint = new QLabel(
        QStringLiteral(
            "Komendy wysyłane na port po kliknięciu Załaduj / Wyładuj w zakładce Sterowanie. "
            "Użyj \\n na końcu linii, jeśli ploter tego wymaga (np. PG;\\n). "
            "Wymagane połączenie z ploterem."),
        page);
    hint->setWordWrap(true);

    form->addRow(trInk("Załaduj materiał"), load_cmd);
    form->addRow(trInk("Wyładuj materiał"), unload_cmd);
    form->addRow(hint);

    QObject::connect(load_cmd, &QLineEdit::textChanged, page,
                     [&app](const QString& t) { app.material_load_command = t; });
    QObject::connect(unload_cmd, &QLineEdit::textChanged, page,
                     [&app](const QString& t) { app.material_unload_command = t; });

    return page;
}

} // namespace

bool runSettingsDialog(QWidget* parent, AppSettings& app, PlotJobSettings& job)
{
    AppSettings edited = app;
    edited.flatten_step = job.flatten_step;

    QDialog dlg(parent);
    dlg.setWindowTitle(QStringLiteral("Ustawienia — Inkcut"));

    const UiProfileMetrics metrics = metricsFor(edited.ui_profile);
    applyDialogProfile(&dlg, metrics.settings_dialog, edited.ui_profile);

    auto* header = new QLabel(QStringLiteral("Ustawienia"), &dlg);
    header->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: bold; color: #1565c0;"));

    auto* nav = new QListWidget(&dlg);
    nav->setMaximumWidth(metrics.settings_nav_max_width);
    nav->addItem(QStringLiteral("Zadanie"));
    nav->addItem(trInk("Podgląd"));
    nav->addItem(QStringLiteral("System"));
    nav->addItem(QStringLiteral("Sterowanie"));

    auto* stack = new QStackedWidget(&dlg);
    stack->addWidget(makeJobPage(stack, edited));
    stack->addWidget(makePreviewPage(stack, edited));
    stack->addWidget(makeSystemPage(stack, edited));
    stack->addWidget(makeControlPage(stack, edited));

    QObject::connect(nav, &QListWidget::currentRowChanged, stack, &QStackedWidget::setCurrentIndex);
    nav->setCurrentRow(0);

    auto* body = new QHBoxLayout();
    body->addWidget(nav);
    body->addWidget(stack, 1);

    auto* buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    auto* root = new QVBoxLayout(&dlg);
    root->addWidget(header);
    root->addLayout(body, 1);
    root->addWidget(buttons);

    if (dlg.exec() != QDialog::Accepted)
        return false;

    app = edited;
    job.flatten_step = edited.flatten_step;
    return true;
}

} // namespace inkcut
