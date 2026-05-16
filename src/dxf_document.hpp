// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QPainterPath>
#include <QString>

namespace inkcut {

/// Parsuje DXF (ASCII lub **binary**) → jedna ścieżka złożona z ENCJI.
/// LINE, ARC, CIRCLE, LWPOLYLINE, POLYLINE/VERTEX (bulge), SPLINE (fit/control ↔ polylinia),
/// ELLIPSE (próbkowanie), INSERT z blokami (rekursja z limitem głębokości).
bool loadDxfPainterPathFromBytes(const QByteArray& raw, QPainterPath& out,
                                 QString* error_message = nullptr);

inline bool loadDxfPainterPath(const QString& ascii, QPainterPath& out, QString* error_message = nullptr)
{
    return loadDxfPainterPathFromBytes(ascii.toLatin1(), out, error_message);
}

} // namespace inkcut
