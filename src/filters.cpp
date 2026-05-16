// SPDX-License-Identifier: GPL-3.0-or-later

#include "filters.hpp"

#include "path_utils.hpp"

#include <QLineF>
#include <QPolygonF>
#include <QRectF>
#include <QSizeF>
#include <QTransform>
#include <QVector2D>
#include <cmath>
#include <optional>

namespace inkcut {

QPainterPath applyRepeatFilter(const QPainterPath& path, const RepeatFilterConfig& cfg)
{
    if (cfg.steps <= 1)
        return path;

    auto parts = splitPainterPath(path);
    QPainterPath result;
    const double max_gap_2 = cfg.closed_loop_distance * cfg.closed_loop_distance;

    for (const QPainterPath& part : parts) {
        if (part.elementCount() == 0)
            continue;
        const auto e0 = part.elementAt(0);
        const auto e1 = part.elementAt(part.elementCount() - 1);
        const QVector2D gap(static_cast<float>(e0.x - e1.x), static_cast<float>(e0.y - e1.y));
        const double len2 = QVector2D::dotProduct(gap, gap);

        if (len2 < max_gap_2) {
            result.addPath(part);
            for (int i = 1; i < cfg.steps; ++i)
                result.connectPath(part);
        } else {
            for (int i = 0; i < cfg.steps; ++i)
                result.addPath(part);
        }
    }
    return result;
}

static double normalizeAngle(double angle)
{
    while (angle > 180.0)
        angle -= 360.0;
    while (angle < -180.0)
        angle += 360.0;
    return angle;
}

static double absAngleDelta(double a1, double a2)
{
    return std::abs(normalizeAngle(a1 - a2));
}

QPainterPath applyMinLineFilter(const QPainterPath& path, const MinLineFilterConfig& cfg)
{
    QPainterPath model = path;

    if (cfg.min_jump > 0) {
        std::vector<QPainterPath::Element> result;
        QVector2D last_pos;
        bool has_last = false;
        const double min_jump_sq = cfg.min_jump * cfg.min_jump;

        for (int i = 0; i < model.elementCount(); ++i) {
            const auto e = model.elementAt(i);
            if (e.type == QPainterPath::MoveToElement && has_last) {
                QVector2D p1(static_cast<float>(e.x), static_cast<float>(e.y));
                const QVector2D d = p1 - last_pos;
                if (d.lengthSquared() < static_cast<float>(min_jump_sq))
                    continue;
            }
            result.push_back(e);
            last_pos = QVector2D(static_cast<float>(e.x), static_cast<float>(e.y));
            has_last = true;
        }
        model = pathFromElements(result);
    }

    if (cfg.min_path > 0) {
        auto parts = splitPainterPath(model);
        std::vector<QPainterPath> kept;
        for (const auto& p : parts) {
            if (p.length() >= cfg.min_path)
                kept.push_back(p);
        }
        model = joinPainterPaths(kept);
    }

    if (cfg.min_edge > 0) {
        std::vector<QPainterPath::Element> result;
        QVector2D last_pos;
        bool has_last = false;
        const double min_d_sq = cfg.min_edge * cfg.min_edge;

        for (int i = 0; i < model.elementCount(); ++i) {
            const auto e = model.elementAt(i);
            if (e.type == QPainterPath::LineToElement && has_last && i + 1 < model.elementCount()
                && model.elementAt(i + 1).type != QPainterPath::MoveToElement) {
                QVector2D p1(static_cast<float>(e.x), static_cast<float>(e.y));
                const QVector2D d = p1 - last_pos;
                if (d.lengthSquared() < static_cast<float>(min_d_sq))
                    continue;
            }
            result.push_back(e);
            last_pos = QVector2D(static_cast<float>(e.x), static_cast<float>(e.y));
            has_last = true;
        }
        model = pathFromElements(result);
    }

    if (cfg.min_shift > 0) {
        const double min_d_sq = cfg.min_shift * cfg.min_shift;
        const double ANGLE_CONFIG = 45.0;
        std::optional<double> last_angle;

        QPainterPath result_path;
        QPainterPath tmp_path;
        auto items = pathToElements(model);

        for (int i = 0; i < static_cast<int>(items.size()); ++i) {
            const auto& e = items[i];
            if (e.type == QPainterPath::MoveToElement) {
                addItemToPath(result_path, e, i, items);
                last_angle.reset();
            } else if (e.type == QPainterPath::LineToElement) {
                const QPointF p(e.x, e.y);
                const QPointF seg = p - result_path.currentPosition();
                bool add = true;
                const double segment_l2 = QPointF::dotProduct(seg, seg);

                if (last_angle && i + 1 < static_cast<int>(items.size()) && segment_l2 < min_d_sq) {
                    const auto& next_element = items[i + 1];
                    if (next_element.type != QPainterPath::MoveToElement
                        && next_element.type != QPainterPath::CurveToDataElement) {
                        tmp_path.clear();
                        tmp_path.moveTo(p);
                        addItemToPath(tmp_path, next_element, i + 1, items);
                        const double angle_next = tmp_path.angleAtPercent(0);

                        if (segment_l2 > 0) {
                            const double angle_current =
                                QLineF(result_path.currentPosition(), p).angle();
                            if (absAngleDelta(angle_current, *last_angle)
                                        + absAngleDelta(angle_next, angle_current)
                                    > absAngleDelta(angle_next, *last_angle) + ANGLE_CONFIG)
                                add = false;
                        } else {
                            add = false;
                        }
                    }
                }

                if (add) {
                    result_path.lineTo(p);
                    last_angle = trailingAngle(result_path);
                } else {
                    last_angle.reset();
                }
            } else if (e.type == QPainterPath::CurveToDataElement) {
                continue;
            } else {
                addItemToPath(result_path, e, i, items);
                last_angle = trailingAngle(result_path);
            }
        }
        model = result_path;
    }

    return model;
}

void applyOvercutToClosedPolyline(std::vector<QPointF>& poly, double overcut)
{
    if (overcut <= 0 || poly.size() < 3)
        return;

    QPainterPath trace;
    trace.moveTo(poly[0]);
    for (size_t i = 1; i < poly.size(); ++i)
        trace.lineTo(poly[i]);
    trace.lineTo(poly[0]);

    if (trace.length() <= overcut)
        return;
    const double t = trace.percentAtLength(overcut);
    poly.push_back(trace.pointAtPercent(t));
}

namespace {

const QTransform kIdentity;

void addContinuityCorrection(QPainterPath& offset_path, const QPainterPath& blade_path,
                             const QPointF& point, double offset, double cutoff_deg)
{
    const QPointF cur = blade_path.currentPosition();

    QPainterPath sp;
    sp.moveTo(cur);
    sp.lineTo(point);
    const double next_angle = sp.angleAtPercent(1.0);
    const double angle = trailingAngle(blade_path);

    if (std::isnan(angle) || std::isnan(next_angle))
        return;

    if (std::abs(angle - next_angle) <= cutoff_deg)
        return;

    const double r = offset;
    double diff = next_angle - angle;
    if (diff > 180.0)
        diff -= 360.0;
    if (diff < -180.0)
        diff += 360.0;

    const QPointF circle_size(r, r);
    offset_path.arcTo(QRectF(cur - circle_size, QSizeF(2 * r, 2 * r)), angle, diff);
}

void processMove(QPainterPath& offset_path, QPainterPath& blade_path, const QPointF& p0, double r)
{
    blade_path.moveTo(p0);
    const double angle = trailingAngle(blade_path);

    double dx, dy;
    if (std::isnan(angle)) {
        dx = 0;
        dy = r;
    } else {
        const double a = angle * M_PI / 180.0;
        dx = r * std::cos(a);
        dy = -r * std::sin(a);
    }

    offset_path.moveTo(QPointF(p0.x() + dx, p0.y() + dy));
}

void processLine(QPainterPath& offset_path, QPainterPath& blade_path, const QPointF& p0, double r,
                 double cutoff_deg)
{
    addContinuityCorrection(offset_path, blade_path, p0, r, cutoff_deg);
    blade_path.lineTo(p0);

    const double angle = trailingAngle(blade_path);

    double dx, dy;
    if (std::isnan(angle)) {
        dx = 0;
        dy = r;
    } else {
        const double a = angle * M_PI / 180.0;
        dx = r * std::cos(a);
        dy = -r * std::sin(a);
    }

    offset_path.lineTo(QPointF(p0.x() + dx, p0.y() + dy));
}

void processQuad(QPainterPath& offset_path, QPainterPath& blade_path, const QPointF& p1,
                 const QPointF& p2, double r, double quality, double cutoff_deg)
{
    const QPointF p0 = blade_path.currentPosition();
    addContinuityCorrection(offset_path, blade_path, p1, r, cutoff_deg);

    QPainterPath curve;
    curve.moveTo(p0);
    curve.quadTo(p1, p2);

    QPolygonF polygon;
    if (quality == 1.0) {
        const auto polys = curve.toSubpathPolygons(kIdentity);
        if (!polys.isEmpty())
            polygon = polys.front();
    } else {
        QTransform m = QTransform::fromScale(quality, quality);
        QTransform inv = QTransform::fromScale(1.0 / quality, 1.0 / quality);
        const auto polys = curve.toSubpathPolygons(m);
        if (!polys.isEmpty())
            polygon = inv.map(polys.front());
    }

    QPainterPath accum;
    accum.moveTo(p0);

    for (const QPointF& point : polygon) {
        accum.lineTo(point);
        const double t = curve.percentAtLength(accum.length());
        const double ang = curve.angleAtPercent(t);
        const double a = ang * M_PI / 180.0;
        const double dx = r * std::cos(a);
        const double dy = -r * std::sin(a);
        offset_path.lineTo(point.x() + dx, point.y() + dy);
    }

    blade_path.quadTo(p1, p2);
}

void processCubic(QPainterPath& offset_path, QPainterPath& blade_path, const QPointF& p1,
                  const QPointF& p2, const QPointF& p3, double r, double quality, double cutoff_deg)
{
    const QPointF p0 = blade_path.currentPosition();
    addContinuityCorrection(offset_path, blade_path, p1, r, cutoff_deg);

    QPainterPath curve;
    curve.moveTo(p0);
    curve.cubicTo(p1, p2, p3);

    QPolygonF polygon;
    if (quality == 1.0) {
        const auto polys = curve.toSubpathPolygons(kIdentity);
        if (!polys.isEmpty())
            polygon = polys.front();
    } else {
        QTransform m = QTransform::fromScale(quality, quality);
        QTransform inv = QTransform::fromScale(1.0 / quality, 1.0 / quality);
        const auto polys = curve.toSubpathPolygons(m);
        if (!polys.isEmpty())
            polygon = inv.map(polys.front());
    }

    QPainterPath accum;
    accum.moveTo(p0);

    for (const QPointF& point : polygon) {
        accum.lineTo(point);
        const double t = curve.percentAtLength(accum.length());
        const double ang = curve.angleAtPercent(t);
        const double a = ang * M_PI / 180.0;
        const double dx = r * std::cos(a);
        const double dy = -r * std::sin(a);
        offset_path.lineTo(point.x() + dx, point.y() + dy);
    }

    blade_path.cubicTo(p1, p2, p3);
}

} // namespace

QPainterPath applyBladeOffsetFilter(const QPainterPath& path, const BladeOffsetConfig& cfg)
{
    const double d = cfg.offset;
    if (d <= 0)
        return path;

    QPainterPath blade_path;
    QPainterPath offset_path;

    std::vector<QPointF> params;
    const double qf = cfg.quality_factor;
    const double cutoff = cfg.cutoff_deg;

    auto finish_curve = [&]() {
        const int n = static_cast<int>(params.size());
        if (n == 2)
            processQuad(offset_path, blade_path, params[0], params[1], d, qf, cutoff);
        else if (n == 3)
            processCubic(offset_path, blade_path, params[0], params[1], params[2], d, qf, cutoff);
        params.clear();
    };

    for (int i = 0; i < path.elementCount(); ++i) {
        const auto e = path.elementAt(i);

        if (!params.empty() && e.type != QPainterPath::CurveToDataElement)
            finish_curve();

        switch (e.type) {
        case QPainterPath::MoveToElement:
            processMove(offset_path, blade_path, QPointF(e.x, e.y), d);
            break;
        case QPainterPath::LineToElement:
            processLine(offset_path, blade_path, QPointF(e.x, e.y), d, cutoff);
            break;
        case QPainterPath::CurveToElement:
            params = { QPointF(e.x, e.y) };
            break;
        case QPainterPath::CurveToDataElement:
            params.emplace_back(e.x, e.y);
            break;
        default:
            break;
        }
    }
    if (!params.empty())
        finish_curve();

    return offset_path;
}

} // namespace inkcut
