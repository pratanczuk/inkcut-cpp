// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QPainterPath>

namespace inkcut {

enum class OrderStrategy {
    Normal,
    Reversed,
    MinX,
    MaxX,
    MinY,
    MaxY,
    ShortestPath,
    Hilbert,
    ZCurve,
};

QPainterPath applyCutOrder(const QPainterPath& path, OrderStrategy strategy);

} // namespace inkcut
