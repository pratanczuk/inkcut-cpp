// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QStringList>

#include <QPainterPath>
#include <QString>

namespace inkcut {

/// Parse SVG into one compound painter path (groups, transforms, path/rect/circle/…).
bool loadSvgPainterPath(const QString& xml, QPainterPath& out, QString* error_message = nullptr,
                         const QString& document_base_dir = QString(),
                         QStringList* warnings = nullptr);

} // namespace inkcut
