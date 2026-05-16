// SPDX-License-Identifier: GPL-3.0-or-later

#include "mainwindow.hpp"

#include "app_settings.hpp"
#include "i18n.hpp"
#include "app_theme.hpp"

#include <QApplication>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("inkcut-gui"));

    inkcut::AppSettings settings;
    inkcut::loadAppSettings(settings);
    inkcut::applyApplicationTheme(app, settings.dock_style);
    inkcut::installInkcutTranslator(app, settings.language);

    inkcut::MainWindow w;
    w.applyStartupSettings(settings);
    w.show();
    return app.exec();
}
