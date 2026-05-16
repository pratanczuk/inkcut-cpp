// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QPainterPath>
#include <QPointF>
#include <vector>

namespace inkcut {

struct RepeatFilterConfig {
    int steps = 1;
    double closed_loop_distance = 0.1;
};
QPainterPath applyRepeatFilter(const QPainterPath& path, const RepeatFilterConfig& cfg);

struct MinLineFilterConfig {
    double min_jump = 0;
    double min_path = 0;
    double min_edge = 0;
    double min_shift = 0;
};
QPainterPath applyMinLineFilter(const QPainterPath& path, const MinLineFilterConfig& cfg);

struct BladeOffsetConfig {
    double offset = 0;
    double cutoff_deg = 5.0;
    double quality_factor = 1.0;
};
QPainterPath applyBladeOffsetFilter(const QPainterPath& path, const BladeOffsetConfig& cfg);

/// Overcut on closed contours (first point meets last within epsilon elsewhere in pipeline).
void applyOvercutToClosedPolyline(std::vector<QPointF>& poly, double overcut);

} // namespace inkcut
