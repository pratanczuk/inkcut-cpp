// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "bitmap_trace.hpp"

class QImage;
class QWidget;

namespace inkcut {

/// Dialog parametrów mkbitmap/potrace z podglądem binarizacji. Zwraca false przy Anuluj.
bool runBitmapTraceDialog(QWidget* parent, const QImage& source, BitmapTraceOptions& options);

} // namespace inkcut
