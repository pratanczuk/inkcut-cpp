// SPDX-License-Identifier: GPL-3.0-or-later
// Ported from inkcut/core/utils.py (QPainterPath helpers)

#pragma once

#include <QPainterPath>
#include <QPointF>
#include <vector>

namespace inkcut {

std::vector<QPainterPath> splitPainterPath(const QPainterPath& path);

QPainterPath joinPainterPaths(const std::vector<QPainterPath>& paths);

QPainterPath pathFromElements(const std::vector<QPainterPath::Element>& elements);

std::vector<QPainterPath::Element> pathToElements(const QPainterPath& path);

QPointF pathElementToPoint(const QPainterPath::Element& e);

/// Approximate trailing direction angle as upstream Inkcut (degrees).
double trailingAngle(const QPainterPath& path);

void addItemToPath(QPainterPath& result, const QPainterPath::Element& e, int i,
                   const std::vector<QPainterPath::Element>& items);

} // namespace inkcut
