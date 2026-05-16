// SPDX-License-Identifier: GPL-3.0-or-later

#include "live_plot_view.hpp"

#include <QPainter>
#include <QWheelEvent>
#include <QtMath>

#include <algorithm>

namespace inkcut {

namespace {

/// Krok siatki w mm — nie gęściej niż ~24 px i nie więcej niż maxLines na oś.
double gridStepMm(const QGraphicsView* view, double base_mm, int max_lines = 48)
{
    if (!view || base_mm <= 0)
        return 1.0;

    const QRectF vr = view->mapToScene(view->viewport()->rect()).boundingRect();
    if (vr.isEmpty())
        return base_mm;

    double step = base_mm;
    auto too_dense = [&]() {
        return vr.width() / step > max_lines || vr.height() / step > max_lines;
    };
    while (too_dense() && step < 1e9)
        step *= 2.0;

    const QPointF s0 = view->mapToScene(QPoint(0, 0));
    const QPointF s1 = view->mapToScene(QPoint(24, 0));
    const double scene_per_px = qAbs(s1.x() - s0.x()) / 24.0;
    if (scene_per_px > 1e-9) {
        const double min_step = 24.0 * scene_per_px;
        while (step < min_step && step < 1e9)
            step *= 2.0;
    }

    return step;
}

} // namespace

LivePlotView::LivePlotView(QWidget* parent) : QGraphicsView(parent)
{
    setMinimumSize(200, 140);
    setRenderHint(QPainter::Antialiasing, true);
    setBackgroundBrush(QColor(255, 255, 255));
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);
}

void LivePlotView::setAxisUnitMm(double mm_per_unit)
{
    axis_unit_mm_ = mm_per_unit > 0 ? mm_per_unit : 1.0;
    viewport()->update();
}

void LivePlotView::setGridVisible(bool on)
{
    show_grid_ = on;
    viewport()->update();
}

void LivePlotView::setGridAxes(bool show_x, bool show_y)
{
    show_grid_x_ = show_x;
    show_grid_y_ = show_y;
    viewport()->update();
}

void LivePlotView::setGridAlpha(int alpha)
{
    grid_alpha_ = std::clamp(alpha, 0, 255);
    viewport()->update();
}

void LivePlotView::fitAll()
{
    if (!scene())
        return;
    const QRectF bounds = scene()->itemsBoundingRect();
    if (bounds.isEmpty())
        return;
    const double m = qMax(20.0, qMax(bounds.width(), bounds.height()) * 0.08);
    fitInView(bounds.adjusted(-m, -m, m, m), Qt::KeepAspectRatio);
}

void LivePlotView::zoomBy(double factor)
{
    if (factor <= 0 || !qIsFinite(factor))
        return;
    scale(factor, factor);
}

void LivePlotView::zoomIn()
{
    zoomBy(1.25);
}

void LivePlotView::zoomOut()
{
    zoomBy(1.0 / 1.25);
}

void LivePlotView::wheelEvent(QWheelEvent* event)
{
    if (event->angleDelta().y() != 0) {
        const double factor = std::pow(1.0015, event->angleDelta().y());
        zoomBy(factor);
        event->accept();
        return;
    }
    QGraphicsView::wheelEvent(event);
}

void LivePlotView::drawBackground(QPainter* painter, const QRectF& rect)
{
    QGraphicsView::drawBackground(painter, rect);

    if (!show_grid_ || !scene() || scene()->items().isEmpty())
        return;

    const QRectF vr = mapToScene(viewport()->rect()).boundingRect();
    if (vr.isEmpty() || !vr.isValid())
        return;

    const double step = gridStepMm(this, axis_unit_mm_);
    const QColor grid_color(0, 0, 0, grid_alpha_);

    painter->save();
    painter->setPen(QPen(grid_color, 0));

    if (show_grid_x_) {
        const double x0 = std::floor(vr.left() / step) * step;
        const double x1 = std::ceil(vr.right() / step) * step;
        for (double x = x0; x <= x1; x += step)
            painter->drawLine(QPointF(x, vr.top()), QPointF(x, vr.bottom()));
    }

    if (show_grid_y_) {
        const double y0 = std::floor(vr.top() / step) * step;
        const double y1 = std::ceil(vr.bottom() / step) * step;
        for (double y = y0; y <= y1; y += step)
            painter->drawLine(QPointF(vr.left(), y), QPointF(vr.right(), y));
    }

    painter->restore();
}

void LivePlotView::drawForeground(QPainter* painter, const QRectF& rect)
{
    Q_UNUSED(rect);
    if (!show_grid_ || !scene() || scene()->items().isEmpty())
        return;

    const QRectF vr = mapToScene(viewport()->rect()).boundingRect();
    if (vr.isEmpty() || !vr.isValid())
        return;

    const double step = gridStepMm(this, axis_unit_mm_);
    painter->save();
    painter->setPen(Qt::black);
    QFont f = painter->font();
    f.setPointSize(8);
    painter->setFont(f);

    const int vp_w = viewport()->width();
    const int vp_h = viewport()->height();
    const double x0 = std::floor(vr.left() / step) * step;
    const double x1 = std::ceil(vr.right() / step) * step;
    for (double x = x0; x <= x1; x += step) {
        const QPoint p = mapFromScene(QPointF(x, vr.bottom()));
        if (p.x() < 0 || p.x() > vp_w)
            continue;
        painter->drawText(QRect(p.x() - 28, vp_h - 18, 56, 16), Qt::AlignCenter,
                          QString::number(x, 'g', 4));
    }

    const double y0 = std::floor(vr.top() / step) * step;
    const double y1 = std::ceil(vr.bottom() / step) * step;
    for (double y = y0; y <= y1; y += step) {
        const QPoint p = mapFromScene(QPointF(vr.left(), y));
        if (p.y() < 0 || p.y() > vp_h)
            continue;
        painter->drawText(QRect(2, p.y() - 8, 40, 16), Qt::AlignRight | Qt::AlignVCenter,
                          QString::number(y, 'g', 4));
    }

    painter->restore();
}

} // namespace inkcut
