// SPDX-License-Identifier: GPL-3.0-or-later

#include "app_theme.hpp"

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>

namespace inkcut {

void applyApplicationTheme(QApplication& app, const QString& dock_style)
{
    const QString style = dock_style.trimmed().toLower();
    if (style == QLatin1String("dark")) {
        app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        QPalette pal = app.palette();
        pal.setColor(QPalette::Window, QColor(45, 45, 48));
        pal.setColor(QPalette::WindowText, Qt::white);
        pal.setColor(QPalette::Base, QColor(30, 30, 30));
        pal.setColor(QPalette::AlternateBase, QColor(45, 45, 48));
        pal.setColor(QPalette::ToolTipBase, Qt::white);
        pal.setColor(QPalette::ToolTipText, Qt::white);
        pal.setColor(QPalette::Text, Qt::white);
        pal.setColor(QPalette::Button, QColor(45, 45, 48));
        pal.setColor(QPalette::ButtonText, Qt::white);
        pal.setColor(QPalette::BrightText, Qt::red);
        pal.setColor(QPalette::Highlight, QColor(42, 130, 218));
        pal.setColor(QPalette::HighlightedText, Qt::black);
        app.setPalette(pal);
        return;
    }
    if (style == QLatin1String("light")) {
        app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        app.setPalette(QApplication::style()->standardPalette());
        return;
    }
    app.setStyle(QString());
    app.setPalette(QApplication::style()->standardPalette());
}

} // namespace inkcut
