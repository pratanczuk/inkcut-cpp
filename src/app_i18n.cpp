// SPDX-License-Identifier: GPL-3.0-or-later

#include "app_i18n.hpp"

#include <QHash>

namespace inkcut {

namespace {

QString g_language = QStringLiteral("pl");

QHash<QString, QString> englishMap()
{
    QHash<QString, QString> m;
    m.insert(QStringLiteral("Materiał"), QStringLiteral("Material"));
    m.insert(QStringLiteral("Grafika"), QStringLiteral("Graphic"));
    m.insert(QStringLiteral("Warstwy"), QStringLiteral("Layers"));
    m.insert(QStringLiteral("Linie tnące"), QStringLiteral("Weedlines"));
    m.insert(QStringLiteral("Zadania"), QStringLiteral("Jobs"));
    m.insert(QStringLiteral("Monitor"), QStringLiteral("Monitor"));
    m.insert(QStringLiteral("Konsola"), QStringLiteral("Console"));
    m.insert(QStringLiteral("Sterowanie"), QStringLiteral("Control"));
    m.insert(QStringLiteral("Ustawienia"), QStringLiteral("Settings"));
    m.insert(QStringLiteral("Urządzenie"), QStringLiteral("Device"));
    m.insert(QStringLiteral("Plik"), QStringLiteral("File"));
    m.insert(QStringLiteral("Pomoc"), QStringLiteral("Help"));
    m.insert(QStringLiteral("Otwórz SVG / DXF…"), QStringLiteral("Open SVG / DXF…"));
    m.insert(QStringLiteral("Wyślij na urządzenie…"), QStringLiteral("Send to device…"));
    m.insert(QStringLiteral("Konfiguracja urządzenia…"), QStringLiteral("Device setup…"));
    m.insert(QStringLiteral("Ustawienia…"), QStringLiteral("Settings…"));
    m.insert(QStringLiteral("O programie…"), QStringLiteral("About…"));
    m.insert(QStringLiteral("Przerwij zadanie"), QStringLiteral("Abort job"));
    m.insert(QStringLiteral("Pokaż całość"), QStringLiteral("View all"));
    m.insert(QStringLiteral("Wyczyść wykres"), QStringLiteral("Clear plot"));
    m.insert(QStringLiteral("Cały dokument"), QStringLiteral("Whole document"));
    m.insert(QStringLiteral("Filtry kolorów dostępne tylko dla SVG."),
             QStringLiteral("Color filters are available for SVG only."));
    m.insert(QStringLiteral("Konsola urządzenia — wpisz komendę (HPGL/G-code) i Enter."),
             QStringLiteral("Device console — type a command (HPGL/G-code) and press Enter."));
    return m;
}

} // namespace

void setAppLanguage(const QString& language_code)
{
    const QString l = language_code.trimmed().toLower();
    g_language = (l == QLatin1String("en")) ? QStringLiteral("en") : QStringLiteral("pl");
}

QString appLanguage()
{
    return g_language;
}

QString inkcutTr(const QString& polish)
{
    if (g_language != QLatin1String("en"))
        return polish;
    return englishMap().value(polish, polish);
}

} // namespace inkcut
