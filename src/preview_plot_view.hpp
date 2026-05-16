// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "live_plot_view.hpp"

class QGraphicsItem;

namespace inkcut {

/// Podgląd z siatką; lewy przycisk na grafice przesuwa ją, na tle — przesuwa widok.
class PreviewPlotView final : public LivePlotView {
    Q_OBJECT
public:
    explicit PreviewPlotView(QWidget* parent = nullptr);

    void setGraphicDragTarget(QGraphicsItem* item);

signals:
    void graphicOffsetChanged(qreal x, qreal y);
    void graphicDragFinished();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QGraphicsItem* drag_target_ = nullptr;
    bool dragging_graphic_ = false;
    bool panning_view_ = false;
    QPoint last_mouse_pos_;
    QPointF drag_start_scene_;
    QPointF drag_start_offset_;
};

} // namespace inkcut
