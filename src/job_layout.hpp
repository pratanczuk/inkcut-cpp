// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "job_model.hpp"

#include <QPainterPath>
#include <QRectF>

namespace inkcut {

/// Material frame (sheet outline) for preview.
QPainterPath materialOutlinePath(const MaterialSettings& m);

/// Inner usable area (material minus padding), like upstream Material.padding_path.
QPainterPath materialAvailableAreaPath(const MaterialSettings& m);

/// Device x-y plane (table); at least as large as the material sheet.
QPainterPath deviceAreaPath(const MaterialSettings& material);

/// Graphic transform, copies, weedlines, padding on material — before cut filters.
QPainterPath applyJobLayout(const QPainterPath& optimized_path, const PlotJobSettings& job,
                            QRectF* single_copy_bounds = nullptr);

/// Transformacja wyjścia urządzenia (lustro, skala, swap XY) po układzie joba.
QPainterPath applyDeviceOutputTransform(const QPainterPath& path, const DeviceSetup& device);

} // namespace inkcut
