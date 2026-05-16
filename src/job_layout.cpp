// SPDX-License-Identifier: GPL-3.0-or-later

#include "job_layout.hpp"

#include <QTransform>
#include <array>
#include <algorithm>
#include <cmath>
#include <vector>

namespace inkcut {

namespace {

void addWeedlineRect(QPainterPath& path, const QRectF& bbox, double pad_l, double pad_t, double pad_r,
                     double pad_b)
{
    const double x = bbox.left() - pad_l;
    const double y = bbox.top() - pad_t;
    const double w = bbox.width() + pad_l + pad_r;
    const double h = bbox.height() + pad_t + pad_b;
    if (w > 0 && h > 0)
        path.addRect(x, y, w, h);
}

std::array<int, 2> computeStackSizes(const MaterialSettings& mat, const QRectF& bbox,
                                     double spacing_x, double spacing_y)
{
    const double avail[2] = {mat.width - mat.padding_left - mat.padding_right,
                             mat.height - mat.padding_top - mat.padding_bottom};
    const double size[2] = {std::max(0.01, bbox.width()), std::max(0.01, bbox.height())};
    const double spacing[2] = {spacing_x, spacing_y};
    std::array<int, 2> stack{0, 0};
    double p[2] = {0, 0};
    for (int i = 0; i < 2; ++i) {
        while ((p[i] + size[i]) < avail[i]) {
            stack[static_cast<size_t>(i)]++;
            p[i] += size[i] + spacing[i];
        }
    }
    return stack;
}

std::vector<QPointF> generateCopyOffsets(int copies, const QRectF& bbox, double spacing_x,
                                           double spacing_y, int stack_x)
{
    const double d0 = bbox.width();
    const double d1 = bbox.height();
    std::vector<QPointF> out;
    out.reserve(static_cast<size_t>(copies));
    double p[2] = {0, 0};
    const int row = std::max(1, stack_x);

    while (int(out.size()) < copies) {
        p[0] = 0;
        out.emplace_back(p[0], p[1]);
        if (int(out.size()) >= copies)
            break;
        for (int i = 0; i < row - 1; ++i) {
            p[0] += d0 + spacing_x;
            out.emplace_back(p[0], p[1]);
            if (int(out.size()) >= copies)
                return out;
        }
        p[1] += d1 + spacing_y;
    }
    return out;
}

} // namespace

QPainterPath materialOutlinePath(const MaterialSettings& m)
{
    QPainterPath p;
    p.addRect(0, 0, m.width, m.height);
    return p;
}

QPainterPath materialAvailableAreaPath(const MaterialSettings& m)
{
    const double w = m.width - m.padding_left - m.padding_right;
    const double h = m.height - m.padding_top - m.padding_bottom;
    QPainterPath p;
    if (w > 0 && h > 0)
        p.addRect(m.padding_left, m.padding_top, w, h);
    return p;
}

QPainterPath deviceAreaPath(const MaterialSettings& material)
{
    // Inkcut defaults: 1800×2700 plot units ≈ 20×30 in.
    constexpr double kDefaultW = 1800.0 / 3.5433070866;
    constexpr double kDefaultH = 2700.0 / 3.5433070866;
    const double w = std::max(kDefaultW, material.width);
    const double h = std::max(kDefaultH, material.height);
    QPainterPath p;
    p.addRect(0, 0, w, h);
    return p;
}

QPainterPath applyDeviceOutputTransform(const QPainterPath& path, const DeviceSetup& dev)
{
    if (path.isEmpty())
        return path;

    QPainterPath p = path;
    QRectF bbox = p.boundingRect();

    if (dev.device_scale > 0 && std::abs(dev.device_scale - 1.0) > 1e-9) {
        const QPointF c = bbox.center();
        QTransform t;
        t.translate(c.x(), c.y());
        t.scale(dev.device_scale, dev.device_scale);
        t.translate(-c.x(), -c.y());
        p = t.map(p);
        bbox = p.boundingRect();
    }

    if (dev.mirror_x || dev.mirror_y) {
        QTransform t;
        t.translate(bbox.center().x(), bbox.center().y());
        t.scale(dev.mirror_x ? -1.0 : 1.0, dev.mirror_y ? -1.0 : 1.0);
        t.translate(-bbox.center().x(), -bbox.center().y());
        p = t.map(p);
        bbox = p.boundingRect();
    }

    if (dev.swap_xy) {
        QTransform t;
        t.rotate(90);
        p = t.map(p);
    }

    return p;
}

QPainterPath applyJobLayout(const QPainterPath& optimized_path, const PlotJobSettings& job,
                            QRectF* single_copy_bounds)
{
    if (optimized_path.isEmpty())
        return {};

    const GraphicLayoutSettings& g = job.layout;
    const MaterialSettings& mat = job.material;
    const WeedlineSettings& w = job.weedlines;

    QTransform t;
    if (g.lock_scale) {
        const double s = g.scale_x;
        t.scale(s * (g.mirror_x ? -1.0 : 1.0), s * (g.mirror_y ? -1.0 : 1.0));
    } else {
        t.scale(g.scale_x * (g.mirror_x ? -1.0 : 1.0), g.scale_y * (g.mirror_y ? -1.0 : 1.0));
    }

    QRectF bbox = optimized_path.boundingRect();
    if (g.rotation_deg != 0) {
        const QPointF c = bbox.center();
        QTransform rot;
        rot.translate(c.x(), c.y());
        rot.rotate(g.rotation_deg);
        rot.translate(-c.x(), -c.y());
        t = rot * t;
    }

    QPainterPath single = t.map(optimized_path);

    if (w.copy_weedline)
        addWeedlineRect(single, single.boundingRect(), w.copy_pad_left, w.copy_pad_top,
                        w.copy_pad_right, w.copy_pad_bottom);

    bbox = single.boundingRect();
    const double avail_w = mat.width - mat.padding_left - mat.padding_right;
    const double avail_h = mat.height - mat.padding_top - mat.padding_bottom;

    if (g.auto_scale && bbox.width() > 0 && bbox.height() > 0) {
        double sx_fit = 1;
        double sy_fit = 1;
        if (bbox.width() > avail_w)
            sx_fit = avail_w / bbox.width();
        if (bbox.height() > avail_h)
            sy_fit = avail_h / bbox.height();
        const double s = std::min(sx_fit, sy_fit);
        if (s < 1.0)
            single = QTransform::fromScale(s, s).map(single);
    }

    bbox = single.boundingRect();
    if (single_copy_bounds)
        *single_copy_bounds = bbox;

    const QPointF br = bbox.bottomRight();
    single = QTransform::fromTranslate(-br.x(), -br.y()).map(single);
    bbox = single.boundingRect();

    const std::array<int, 2> stack =
        computeStackSizes(mat, bbox, g.copy_spacing_x, g.copy_spacing_y);

    int copies = std::max(1, g.copies);
    if (g.auto_copies && stack[0] > 0) {
        const int rem = copies % stack[0];
        if (rem != 0)
            copies += stack[0] - rem;
    }

    const std::vector<QPointF> offsets =
        generateCopyOffsets(copies, bbox, g.copy_spacing_x, g.copy_spacing_y, stack[0]);

    QPainterPath model;
    for (const QPointF& off : offsets) {
        // Qt: Y rośnie w dół — kolejne rzędy kopii przesuwamy w +Y (upstream: -Y przy odwróconym podglądzie).
        model.addPath(QTransform::fromTranslate(off.x(), off.y()).map(single));
    }

    if (w.plot_weedline)
        addWeedlineRect(model, model.boundingRect(), w.plot_pad_left, w.plot_pad_top,
                        w.plot_pad_right, w.plot_pad_bottom);

    const QRectF mb = model.boundingRect();

    double target_left = mat.padding_left;
    double target_top = mat.padding_top;

    if (g.align_center_x)
        target_left = mat.padding_left + (avail_w - mb.width()) / 2.0;

    if (g.align_center_y)
        target_top = mat.padding_top + (avail_h - mb.height()) / 2.0;
    else if (g.auto_shift)
        target_top = mat.height - mat.padding_bottom - mb.height();

    if (!g.auto_shift) {
        if (!g.align_center_x)
            target_left += bbox.left();
        if (!g.align_center_y)
            target_top -= bbox.top();
    }

    model = QTransform::fromTranslate(target_left - mb.left(), target_top - mb.top()).map(model);

    if (g.layout_offset_x != 0.0 || g.layout_offset_y != 0.0)
        model = QTransform::fromTranslate(g.layout_offset_x, g.layout_offset_y).map(model);

    if (job.feed_to_end) {
        const QRectF fed = model.boundingRect();
        model.moveTo(QPointF(0, -job.feed_after + fed.top()));
    }

    return model;
}

} // namespace inkcut
