// SPDX-License-Identifier: GPL-3.0-or-later

#include "job_export.hpp"
#include "job_pipeline.hpp"

#include <QCoreApplication>
#include <QRectF>

#include <cstdio>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    inkcut::PlotJobSettings original;
    original.order = inkcut::OrderStrategy::ShortestPath;
    original.flatten_step = 0.25;
    original.protocol.protocol = inkcut::PlotProtocol::DMPL;
    original.protocol.dmpl_mode = 3;

    const QString json = inkcut::exportJobDocumentToJson(QStringLiteral("/demo.svg"),
                                                         QStringLiteral("svg"), QRectF(1, 2, 3, 4),
                                                         original);

    inkcut::PlotJobSettings loaded;
    QString err;
    if (!inkcut::importJobDocumentJson(json.toUtf8(), loaded, nullptr, nullptr, nullptr, &err)) {
        std::fprintf(stderr, "%s\n", qPrintable(err));
        return 1;
    }

    if (loaded.flatten_step != original.flatten_step || loaded.order != original.order
        || loaded.protocol.protocol != original.protocol.protocol
        || loaded.protocol.dmpl_mode != original.protocol.dmpl_mode)
        return 2;

    return 0;
}
