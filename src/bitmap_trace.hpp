// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QImage>
#include <QPainterPath>
#include <QPixmap>
#include <QSize>
#include <QString>

#include <functional>

namespace inkcut {

using TraceProgressFn = std::function<void(int percent)>;

/// Parametry przygotowania bitmapy (jak [mkbitmap](https://potrace.sourceforge.net/mkbitmap.html))
/// oraz potrace. Wartości ujemne oznaczają tryb automatyczny.
struct BitmapTraceOptions {
    /// Promień filtra górnoprzepustowego w px (mkbitmap -f). 0 = wyłączony, -1 = auto.
    int highpass_radius = -1;
    /// Skalowanie przed progowaniem (mkbitmap -s): 1, 2, 3… -1 = auto (2× dla małych skanów).
    int scale_factor = -1;
    /// Próg 0…1 (mkbitmap -t). Ujemny = auto (Otsu + domyślne 0.48).
    double threshold = -1.0;
    /// Potrace turdsize (-t). Ujemny = auto z rozmiaru obrazu.
    int turdsize = -1;
    /// Minimalna powierzchnia plamki do usunięcia przed śledzeniem. Ujemna = auto.
    int despeckle_min_area = -1;
    /// Wymuś odwrócenie jasności przed filtrem (ciemne tło → jasne).
    bool force_invert = false;
    /// Morfologiczne domykanie: wypełnia cienkie białe szczeliny wewnątrz kształtu (px, 0 = wył.).
    int gap_close_radius = 0;
    /// Wypełnia zamknięte białe „dziury” o powierzchni ≤ N px (0 = wył.).
    int fill_holes_max_area = 0;
};

/// Podgląd binarnej mapy po filtrach (mkbitmap).
QImage previewBitmapForTrace(const QImage& source, const BitmapTraceOptions& options);

/// Podgląd wektoryzacji (ten sam pipeline co import: prepare + potrace).
QPixmap renderBitmapTracePreview(const QImage& source, const BitmapTraceOptions& options,
                                 const QSize& target_size);

/// Wektoruje bitmapę (ciemne kształty na jasnym tle) do ścieżki w pikselach obrazu.
bool traceBitmapToPath(const QImage& image, QPainterPath& out, QString* error_message = nullptr,
                       const TraceProgressFn& progress = nullptr,
                       const BitmapTraceOptions& options = {});

/// Bezpośrednio do SVG (krzywe Béziera z potrace, fill-rule evenodd dla dziur).
bool traceBitmapToSvg(const QImage& image, QString& svg_xml_out,
                      QString* error_message = nullptr, const TraceProgressFn& progress = nullptr,
                      const BitmapTraceOptions& options = {});

bool isBitmapTracingAvailable();

} // namespace inkcut
