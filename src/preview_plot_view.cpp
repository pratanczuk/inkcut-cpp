// SPDX-License-Identifier: GPL-3.0-or-later

#include "preview_plot_view.hpp"

#include <QGraphicsItem>
#include <QMouseEvent>
#include <QScrollBar>

namespace inkcut {

PreviewPlotView::PreviewPlotView(QWidget* parent) : LivePlotView(parent)
{
    setDragMode(QGraphicsView::NoDrag);
}

void PreviewPlotView::setGraphicDragTarget(QGraphicsItem* item)
{
    drag_target_ = item;
}

void PreviewPlotView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && drag_target_ && drag_target_->isVisible()) {
        const QPointF scene_pos = mapToScene(event->pos());
        if (drag_target_->contains(drag_target_->mapFromScene(scene_pos))) {
            dragging_graphic_ = true;
            panning_view_ = false;
            drag_start_scene_ = scene_pos;
            drag_start_offset_ = drag_target_->pos();
            last_mouse_pos_ = event->pos();
            setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }
    }

    if (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton) {
        panning_view_ = true;
        dragging_graphic_ = false;
        last_mouse_pos_ = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    LivePlotView::mousePressEvent(event);
}

void PreviewPlotView::mouseMoveEvent(QMouseEvent* event)
{
    if (dragging_graphic_ && drag_target_) {
        const QPointF pos =
            drag_start_offset_ + (mapToScene(event->pos()) - drag_start_scene_);
        drag_target_->setPos(pos);
        emit graphicOffsetChanged(pos.x(), pos.y());
        last_mouse_pos_ = event->pos();
        event->accept();
        return;
    }

    if (panning_view_) {
        const QPoint delta = event->pos() - last_mouse_pos_;
        last_mouse_pos_ = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
        return;
    }

    if (drag_target_ && drag_target_->isVisible()) {
        const QPointF scene_pos = mapToScene(event->pos());
        if (drag_target_->contains(drag_target_->mapFromScene(scene_pos)))
            setCursor(Qt::OpenHandCursor);
        else
            unsetCursor();
    }

    LivePlotView::mouseMoveEvent(event);
}

void PreviewPlotView::mouseReleaseEvent(QMouseEvent* event)
{
    if (dragging_graphic_) {
        dragging_graphic_ = false;
        unsetCursor();
        emit graphicDragFinished();
        event->accept();
        return;
    }

    if (panning_view_) {
        panning_view_ = false;
        unsetCursor();
        event->accept();
        return;
    }

    LivePlotView::mouseReleaseEvent(event);
}

} // namespace inkcut
