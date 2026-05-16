// SPDX-License-Identifier: GPL-3.0-or-later

#include "i18n.hpp"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QLibraryInfo>
#include <QTranslator>

namespace inkcut {

void installInkcutTranslator(QApplication& app, const QString& language_code)
{
    static QTranslator s_builtin;
    static QTranslator s_qt;

    app.removeTranslator(&s_builtin);
    app.removeTranslator(&s_qt);

    const QString lang = language_code.trimmed().toLower();
    if (lang.isEmpty() || lang == QLatin1String("pl"))
        return;

    const QString qm_name = QStringLiteral("inkcut_%1.qm").arg(lang);
    const QStringList search_dirs = {
        QStringLiteral(":/i18n"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/translations"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../translations"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../share/inkcut/translations"),
    };

    bool loaded = false;
    for (const QString& dir : search_dirs) {
        if (s_builtin.load(qm_name, dir)) {
            app.installTranslator(&s_builtin);
            loaded = true;
            break;
        }
    }

#ifndef NDEBUG
    if (!loaded)
        qWarning() << "Inkcut: nie wczytano tłumaczenia" << qm_name << "z" << search_dirs;
#endif

    if (s_qt.load(QStringLiteral("qt_%1").arg(lang),
                  QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        app.installTranslator(&s_qt);
    }
}

} // namespace inkcut
