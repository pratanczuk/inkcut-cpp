// SPDX-License-Identifier: GPL-3.0-or-later

#include "job_export.hpp"
#include "job_pipeline.hpp"
#include "svg_document.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPainterPath>
#include <QRectF>
#include <QTemporaryDir>

#include <cstdio>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    QTemporaryDir dir;
    if (!dir.isValid())
        return 10;

    const QString svg_path =
        QDir(dir.path()).absoluteFilePath(QStringLiteral("replay_smoke.svg"));
    QFile wf(svg_path);
    if (!wf.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return 11;
    wf.write(R"(<svg xmlns="http://www.w3.org/2000/svg"><rect width="10" height="10"/></svg>)");
    wf.close();

    QPainterPath outline;
    QString err;
    QFile rf(svg_path);
    if (!rf.open(QIODevice::ReadOnly))
        return 12;
    const QByteArray raw = rf.readAll();
    if (!inkcut::loadSvgPainterPath(QString::fromUtf8(raw), outline, &err,
                                   QFileInfo(svg_path).absolutePath(), nullptr)) {
        std::fprintf(stderr, "%s\n", qPrintable(err));
        return 13;
    }

    inkcut::PlotJobSettings job;
    job.protocol.protocol = inkcut::PlotProtocol::HPGL;

    const QString json =
        inkcut::exportJobDocumentToJson(QFileInfo(svg_path).absoluteFilePath(),
                                       QStringLiteral("svg"), outline.boundingRect(), job);

    inkcut::PlotJobSettings loaded;
    QString source_path;
    QString kind;
    QRectF bounds;
    if (!inkcut::importJobDocumentJson(json.toUtf8(), loaded, &source_path, &kind, &bounds, &err)) {
        std::fprintf(stderr, "%s\n", qPrintable(err));
        return 14;
    }

    if (kind != QLatin1String("svg"))
        return 15;

    QFile df(source_path);
    if (!df.open(QIODevice::ReadOnly)) {
        std::fprintf(stderr, "missing source: %s\n", qPrintable(source_path));
        return 16;
    }
    const QByteArray draw = df.readAll();

    QPainterPath combined;
    if (!inkcut::loadSvgPainterPath(QString::fromUtf8(draw), combined, &err,
                                   QFileInfo(source_path).absolutePath(), nullptr)) {
        std::fprintf(stderr, "%s\n", qPrintable(err));
        return 17;
    }

    const std::string prog = inkcut::buildPlotProgram(combined, loaded);
    if (prog.empty())
        return 18;

    Q_UNUSED(bounds);
    Q_UNUSED(app);
    return 0;
}
