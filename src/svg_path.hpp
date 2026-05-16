// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QPainterPath>
#include <QString>
#include <QStringView>
#include <QPointF>
#include <vector>

namespace inkcut {

/// Append SVG path d (M L H V C S Q T A Z + relatives) to an existing painter path (current point continues).
bool append_svg_path_d(QPainterPath& path, QStringView d);

/// Approximate path as polylines (pen-down chains). step is max distance between samples on curves (same units as path).
std::vector<std::vector<QPointF>> path_to_polylines(const QPainterPath& path, double flatness_step);

struct MoveCutPaths {
    QPainterPath move;
    QPainterPath cut;
};

/// Travel moves (pen up) vs cutting strokes — jak `Job.move_path` / `Job.cut_path` w upstream.
MoveCutPaths splitMoveCutPaths(const QPainterPath& path);

/// Load all <path d="..."> elements from an SVG XML document into one path (best-effort).
bool append_paths_from_svg_xml(const QString& xml, QPainterPath& out, QString* error_message);

} // namespace inkcut
