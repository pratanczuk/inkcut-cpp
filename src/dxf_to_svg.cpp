// SPDX-License-Identifier: GPL-3.0-or-later

#include "dxf_to_svg.hpp"

#include <QDir>
#include <QFile>
#include <QTemporaryFile>

#ifdef INKCUT_HAVE_DXFRW
#include "drw_entities.h"
#include "drw_interface.h"
#include "libdxfrw.h"
#endif

#include <cmath>

#include "i18n.hpp"

namespace inkcut {

bool isDxfToSvgAvailable()
{
#ifdef INKCUT_HAVE_DXFRW
    return true;
#else
    return false;
#endif
}

#ifdef INKCUT_HAVE_DXFRW

namespace {

QString pathMove(double x, double y)
{
    return QStringLiteral("M %1 %2").arg(x, 0, 'g', 8).arg(y, 0, 'g', 8);
}

QString pathLine(double x, double y)
{
    return QStringLiteral("L %1 %2").arg(x, 0, 'g', 8).arg(y, 0, 'g', 8);
}

class DxfSvgBuilder final : public DRW_Interface {
public:
    QStringList path_d;
    double min_x = 1e300;
    double min_y = 1e300;
    double max_x = -1e300;
    double max_y = -1e300;

    void addHeader(const DRW_Header*) override {}
    void addLType(const DRW_LType&) override {}
    void addLayer(const DRW_Layer&) override {}
    void addDimStyle(const DRW_Dimstyle&) override {}
    void addVport(const DRW_Vport&) override {}
    void addTextStyle(const DRW_Textstyle&) override {}
    void addAppId(const DRW_AppId&) override {}
    void addBlock(const DRW_Block&) override {}
    void setBlock(const int) override {}
    void endBlock() override {}
    void addPoint(const DRW_Point& d) override { expand(d.basePoint.x, d.basePoint.y); }
    void addRay(const DRW_Ray&) override {}
    void addXline(const DRW_Xline&) override {}
    void addKnot(const DRW_Entity&) override {}
    void addInsert(const DRW_Insert&) override {}
    void addTrace(const DRW_Trace&) override {}
    void add3dFace(const DRW_3Dface&) override {}
    void addSolid(const DRW_Solid&) override {}
    void addMText(const DRW_MText&) override {}
    void addText(const DRW_Text&) override {}
    void addDimAlign(const DRW_DimAligned*) override {}
    void addDimLinear(const DRW_DimLinear*) override {}
    void addDimRadial(const DRW_DimRadial*) override {}
    void addDimDiametric(const DRW_DimDiametric*) override {}
    void addDimAngular(const DRW_DimAngular*) override {}
    void addDimAngular3P(const DRW_DimAngular3p*) override {}
    void addDimOrdinate(const DRW_DimOrdinate*) override {}
    void addLeader(const DRW_Leader*) override {}
    void addHatch(const DRW_Hatch*) override {}
    void addViewport(const DRW_Viewport&) override {}
    void addImage(const DRW_Image*) override {}
    void linkImage(const DRW_ImageDef*) override {}
    void addComment(const char*) override {}
    void addPlotSettings(const DRW_PlotSettings*) override {}
    void writeHeader(DRW_Header&) override {}
    void writeBlocks() override {}
    void writeBlockRecords() override {}
    void writeEntities() override {}
    void writeLTypes() override {}
    void writeLayers() override {}
    void writeTextstyles() override {}
    void writeVports() override {}
    void writeDimstyles() override {}
    void writeObjects() override {}
    void writeAppId() override {}

    void expand(double x, double y)
    {
        min_x = std::min(min_x, x);
        min_y = std::min(min_y, y);
        max_x = std::max(max_x, x);
        max_y = std::max(max_y, y);
    }

    void pushPath(const QString& d)
    {
        if (!d.isEmpty())
            path_d.append(d);
    }

    void addLine(const DRW_Line& d) override
    {
        expand(d.basePoint.x, d.basePoint.y);
        expand(d.secPoint.x, d.secPoint.y);
        QString p = pathMove(d.basePoint.x, d.basePoint.y);
        p += pathLine(d.secPoint.x, d.secPoint.y);
        pushPath(p);
    }

    void addCircle(const DRW_Circle& d) override
    {
        expand(d.basePoint.x - d.radious, d.basePoint.y - d.radious);
        expand(d.basePoint.x + d.radious, d.basePoint.y + d.radious);
        const double cx = d.basePoint.x;
        const double cy = d.basePoint.y;
        const double r = d.radious;
        QString p = pathMove(cx + r, cy);
        p += QStringLiteral("A %1 %1 0 1 0 %2 %3 A %1 %1 0 1 0 %4 %5")
                 .arg(r, 0, 'g', 8)
                 .arg(cx - r, 0, 'g', 8)
                 .arg(cy, 0, 'g', 8)
                 .arg(cx + r, 0, 'g', 8)
                 .arg(cy, 0, 'g', 8);
        pushPath(p);
    }

    void addArc(const DRW_Arc& d) override
    {
        const double cx = d.basePoint.x;
        const double cy = d.basePoint.y;
        const double r = d.radious;
        const double a0 = d.staangle * M_PI / 180.0;
        const double a1 = d.endangle * M_PI / 180.0;
        const double x0 = cx + r * std::cos(a0);
        const double y0 = cy + r * std::sin(a0);
        const double x1 = cx + r * std::cos(a1);
        const double y1 = cy + r * std::sin(a1);
        expand(x0, y0);
        expand(x1, y1);
        double da = a1 - a0;
        while (da < 0)
            da += 2 * M_PI;
        const int large = da > M_PI ? 1 : 0;
        QString p = pathMove(x0, y0);
        p += QStringLiteral("A %1 %1 0 %2 1 %3 %4")
                 .arg(r, 0, 'g', 8)
                 .arg(large)
                 .arg(x1, 0, 'g', 8)
                 .arg(y1, 0, 'g', 8);
        pushPath(p);
    }

    void addEllipse(const DRW_Ellipse& d) override
    {
        const double cx = d.basePoint.x;
        const double cy = d.basePoint.y;
        const double mx = d.secPoint.x;
        const double my = d.secPoint.y;
        const double rx = std::hypot(mx - cx, my - cy);
        const double ry = rx * d.ratio;
        expand(cx - rx, cy - ry);
        expand(cx + rx, cy + ry);
        QString p = pathMove(cx + rx, cy);
        p += QStringLiteral("A %1 %2 0 1 0 %3 %4 A %1 %2 0 1 0 %5 %6")
                 .arg(rx, 0, 'g', 8)
                 .arg(ry, 0, 'g', 8)
                 .arg(cx - rx, 0, 'g', 8)
                 .arg(cy, 0, 'g', 8)
                 .arg(cx + rx, 0, 'g', 8)
                 .arg(cy, 0, 'g', 8);
        pushPath(p);
    }

    void addLWPolyline(const DRW_LWPolyline& d) override
    {
        if (d.vertlist.empty())
            return;
        QString p;
        bool first = true;
        for (const auto& v : d.vertlist) {
            expand(v->x, v->y);
            if (first) {
                p = pathMove(v->x, v->y);
                first = false;
            } else {
                p += pathLine(v->x, v->y);
            }
        }
        if (d.flags & 0x1)
            p += QStringLiteral("Z");
        pushPath(p);
    }

    void addPolyline(const DRW_Polyline& d) override
    {
        if (d.vertlist.empty())
            return;
        QString p;
        bool first = true;
        for (const auto& v : d.vertlist) {
            expand(v->basePoint.x, v->basePoint.y);
            if (first) {
                p = pathMove(v->basePoint.x, v->basePoint.y);
                first = false;
            } else {
                p += pathLine(v->basePoint.x, v->basePoint.y);
            }
        }
        if (d.flags & 0x1)
            p += QStringLiteral("Z");
        pushPath(p);
    }

    void addSpline(const DRW_Spline* s) override
    {
        if (!s || s->controllist.size() < 2)
            return;
        QString p = pathMove(s->controllist[0]->x, s->controllist[0]->y);
        for (size_t i = 1; i < s->controllist.size(); ++i) {
            expand(s->controllist[i]->x, s->controllist[i]->y);
            p += pathLine(s->controllist[i]->x, s->controllist[i]->y);
        }
        pushPath(p);
    }

    QString toSvg() const
    {
        double w = max_x - min_x;
        double h = max_y - min_y;
        if (!(w > 0) || !(h > 0)) {
            w = 100;
            h = 100;
        }
        QString body;
        for (const QString& pd : path_d)
            body += QStringLiteral("<path fill=\"none\" stroke=\"#000\" d=\"%1\"/>\n").arg(pd);
        return QStringLiteral(
                   "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                   "<svg xmlns=\"http://www.w3.org/2000/svg\" "
                   "viewBox=\"%1 %2 %3 %4\" width=\"%3\" height=\"%4\">\n"
                   "<g fill=\"none\" stroke=\"#000000\" stroke-width=\"0.25\">\n%5"
                   "</g>\n</svg>\n")
            .arg(min_x, 0, 'g', 6)
            .arg(min_y, 0, 'g', 6)
            .arg(w, 0, 'g', 6)
            .arg(h, 0, 'g', 6)
            .arg(body);
    }
};

} // namespace

#endif

bool convertDxfBytesToSvg(const QByteArray& dxf_raw, QString& svg_xml_out, QString* error_message)
{
    svg_xml_out.clear();
#ifndef INKCUT_HAVE_DXFRW
    if (error_message) {
        *error_message = QStringLiteral(
            "Brak libdxfrw — włącz INKCUT_USE_DXFRW i przebuduj (pobiera się przy konfiguracji CMake).");
    }
    return false;
#else
    if (dxf_raw.isEmpty()) {
        if (error_message)
            *error_message = QStringLiteral("Pusty plik DXF.");
        return false;
    }

    QTemporaryFile tmp(QDir::tempPath() + QStringLiteral("/inkcut_XXXXXX.dxf"));
    tmp.setAutoRemove(true);
    if (!tmp.open()) {
        if (error_message)
            *error_message = trInk("Nie można utworzyć pliku tymczasowego dla DXF.");
        return false;
    }
    if (tmp.write(dxf_raw) != dxf_raw.size()) {
        if (error_message)
            *error_message = trInk("Zapis DXF tymczasowego nie powiódł się.");
        return false;
    }
    tmp.close();

    DxfSvgBuilder builder;
    dxfRW reader(tmp.fileName().toUtf8().constData());
    const bool ok = reader.read(&builder, false);
    if (!ok) {
        if (error_message)
            *error_message = trInk("libdxfrw: odczyt DXF nie powiódł się.");
        return false;
    }

    svg_xml_out = builder.toSvg();
    if (svg_xml_out.isEmpty()) {
        if (error_message)
            *error_message = QStringLiteral("DXF nie zawiera geometrii do konwersji.");
        return false;
    }
    return true;
#endif
}

} // namespace inkcut
