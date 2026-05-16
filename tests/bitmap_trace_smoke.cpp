// SPDX-License-Identifier: GPL-3.0-or-later

#include "bitmap_trace.hpp"
#include "svg_document.hpp"
#include "job_pipeline.hpp"
#include "job_model.hpp"

#include <QCoreApplication>
#include <QImage>
#include <QtGlobal>

#include <iostream>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    if (!inkcut::isBitmapTracingAvailable())
        return 0;

    const QString fixture = QStringLiteral(INKCUT_WHEEL_FIXTURE);
    QImage img(fixture);
    if (img.isNull()) {
        std::cerr << "skip: cannot load " << fixture.toStdString() << "\n";
        return 0;
    }

    QString svg;
    if (!inkcut::traceBitmapToSvg(img, svg, nullptr) || svg.size() < 5000) {
        std::cerr << "trace failed or svg too small: " << svg.size() << "\n";
        return 1;
    }

    QPainterPath path;
    QString err;
    inkcut::PlotJobSettings job;
    if (!inkcut::loadSvgDesignPath(svg, QString(), job, path, &err, nullptr)) {
        std::cerr << err.toStdString() << "\n";
        return 2;
    }

    // loadSvgDesignPath zwraca współrzędne w mm (jak reszta pipeline’u SVG).
    constexpr double kMmPerPlotUnit = 25.4 / 90.0;
    const QRectF b = path.boundingRect();
    if (b.width() < 80 * kMmPerPlotUnit || b.height() < 50 * kMmPerPlotUnit) {
        std::cerr << "bounds too small: " << b.width() << "x" << b.height() << "\n";
        return 3;
    }

    return 0;
}
