// SPDX-License-Identifier: GPL-3.0-or-later

#include "plot_live_position.hpp"

#include "grbl_comm.hpp"

#include <QRegularExpression>

namespace inkcut {

std::optional<LivePosition> parseLivePositionFromRx(const QByteArray& rx_accum,
                                                     PlotProtocol protocol,
                                                     double plot_scale)
{
    Q_UNUSED(plot_scale);
    const QString s = QString::fromLatin1(rx_accum);
    Q_UNUSED(protocol);
    // Prefer status frames (<...|WPos:x,y,z|...>) for GRBL/grblHAL.
    int start = s.lastIndexOf(QLatin1Char('<'));
    if (start >= 0) {
        int end = s.indexOf(QLatin1Char('>'), start);
        if (end > start) {
            GrblStatusFrame st;
            if (parseGrblStatusLine(s.mid(start, end - start + 1), st) && st.has_wpos) {
                LivePosition p;
                p.x = st.wpos_x;
                p.y = st.wpos_y;
                return p;
            }
        }
    }

    static const QRegularExpression gxy(
        QStringLiteral(R"(\bX\s*([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?)\s*Y\s*([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?))"),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch last;
    auto it = gxy.globalMatch(s);
    while (it.hasNext())
        last = it.next();
    if (!last.hasMatch())
        return std::nullopt;

    LivePosition p;
    p.x = last.captured(1).toDouble();
    p.y = last.captured(2).toDouble();
    return p;
}

} // namespace inkcut
