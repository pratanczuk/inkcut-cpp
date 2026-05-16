// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QGraphicsView>

class QWheelEvent;

namespace inkcut {

/// Wykres postępu na żywo (siatka + osie jak `PlotView` w upstream).
class LivePlotView : public QGraphicsView {
    Q_OBJECT
public:
    explicit LivePlotView(QWidget* parent = nullptr);

    void setAxisUnitMm(double mm_per_unit);
    void setGridVisible(bool on);
    void setGridAxes(bool show_x, bool show_y);
    void setGridAlpha(int alpha);
    void fitAll();
    void zoomIn();
    void zoomOut();
    void zoomBy(double factor);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    void drawForeground(QPainter* painter, const QRectF& rect) override;

private:
    double axis_unit_mm_ = 1.0;
    bool show_grid_ = true;
    bool show_grid_x_ = true;
    bool show_grid_y_ = true;
    int grid_alpha_ = 80;
};

} // namespace inkcut
