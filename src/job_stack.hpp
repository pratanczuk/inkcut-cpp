// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "job_model.hpp"

#include <QRectF>

namespace inkcut {

int computeStackCountX(const MaterialSettings& mat, const QRectF& copy_bbox, double spacing_x,
                       double spacing_y);

void applyAddStack(int& copies, int stack_x);

void applyRemoveStack(int& copies, int stack_x);

} // namespace inkcut
