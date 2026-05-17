// SPDX-License-Identifier: GPL-3.0-or-later

#include "grbl_comm.hpp"

#include <QLocale>
#include <QStringList>

namespace inkcut {

namespace {

bool parseDoubleC(const QString& s, double& out)
{
    bool ok = false;
    out = QLocale::c().toDouble(s.trimmed(), &ok);
    return ok;
}

} // namespace

bool isGrblWelcomeLine(const QString& line)
{
    const QString t = line.trimmed();
    return t.startsWith(QStringLiteral("grblHAL "), Qt::CaseInsensitive)
           || t.startsWith(QStringLiteral("Grbl "), Qt::CaseInsensitive);
}

bool isGrblOkLine(const QString& line)
{
    return line.trimmed().compare(QStringLiteral("ok"), Qt::CaseInsensitive) == 0;
}

bool isGrblErrorLine(const QString& line, QString* error_message)
{
    const QString t = line.trimmed();
    if (t.startsWith(QStringLiteral("error:"), Qt::CaseInsensitive)
        || t.startsWith(QStringLiteral("ALARM:"), Qt::CaseInsensitive)) {
        if (error_message)
            *error_message = t;
        return true;
    }
    return false;
}

bool parseGrblSettingLine(const QString& line, int& setting_id, double& setting_value)
{
    const QString t = line.trimmed();
    if (!t.startsWith(QLatin1Char('$')))
        return false;
    const int eq = t.indexOf(QLatin1Char('='));
    if (eq < 2)
        return false;
    bool ok = false;
    setting_id = t.mid(1, eq - 1).toInt(&ok);
    if (!ok)
        return false;
    return parseDoubleC(t.mid(eq + 1), setting_value);
}

bool parseGrblStatusLine(const QString& line, GrblStatusFrame& out)
{
    const QString t = line.trimmed();
    if (!t.startsWith(QLatin1Char('<')) || !t.endsWith(QLatin1Char('>')))
        return false;
    const QString body = t.mid(1, t.size() - 2);
    const QStringList parts = body.split(QLatin1Char('|'), Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return false;

    out = GrblStatusFrame{};
    out.state = parts[0].trimmed();
    out.alarm = out.state.compare(QStringLiteral("ALARM"), Qt::CaseInsensitive) == 0;

    for (int i = 1; i < parts.size(); ++i) {
        const QString p = parts[i].trimmed();
        if (p.startsWith(QStringLiteral("WPos:"), Qt::CaseInsensitive)) {
            const QStringList xyz = p.mid(5).split(QLatin1Char(','));
            if (xyz.size() >= 2) {
                double x = 0.0;
                double y = 0.0;
                if (parseDoubleC(xyz[0], x) && parseDoubleC(xyz[1], y)) {
                    out.has_wpos = true;
                    out.wpos_x = x;
                    out.wpos_y = y;
                }
            }
        } else if (p.startsWith(QStringLiteral("Bf:"), Qt::CaseInsensitive)) {
            const QStringList vals = p.mid(3).split(QLatin1Char(','));
            if (vals.size() >= 2) {
                bool ok1 = false;
                bool ok2 = false;
                const int planner = vals[0].trimmed().toInt(&ok1);
                const int rx = vals[1].trimmed().toInt(&ok2);
                if (ok1 && ok2) {
                    out.has_bf = true;
                    out.bf_planner_free = planner;
                    out.bf_rx_free = rx;
                }
            }
        }
    }
    return true;
}

} // namespace inkcut
