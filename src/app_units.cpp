// SPDX-License-Identifier: GPL-3.0-or-later

#include "app_units.hpp"

namespace inkcut {

namespace {

double mmPerUnit(const QString& units)
{
    const QString u = units.trimmed().toLower();
    if (u == QLatin1String("in"))
        return 25.4;
    if (u == QLatin1String("cm"))
        return 10.0;
    if (u == QLatin1String("m"))
        return 1000.0;
    if (u == QLatin1String("px"))
        return 25.4 / 90.0;
    return 1.0;
}

} // namespace

double mmToDisplay(double mm, const QString& units)
{
    const double f = mmPerUnit(units);
    return f > 0 ? mm / f : mm;
}

double displayToMm(double display, const QString& units)
{
    return display * mmPerUnit(units);
}

QString unitSuffix(const QString& units)
{
    const QString u = units.trimmed().toLower();
    if (u.isEmpty() || u == QLatin1String("mm"))
        return QStringLiteral(" mm");
    return QStringLiteral(" ") + u;
}

QString unitLabel(const QString& units)
{
    const QString u = units.trimmed().toLower();
    if (u.isEmpty())
        return QStringLiteral("mm");
    return u;
}

} // namespace inkcut
