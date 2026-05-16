// SPDX-License-Identifier: GPL-3.0-or-later

#include "svg_path.hpp"

#include "svg_document.hpp"

#include <QLocale>
#include <QLineF>
#include <algorithm>
#include <cmath>

namespace inkcut {

namespace {

bool parse_number(QStringView in, int& i, double& out)
{
    while (i < static_cast<int>(in.size()) && in[i].isSpace())
        ++i;
    if (i >= static_cast<int>(in.size()))
        return false;

    const int start = i;
    if (in[i] == QLatin1Char('+') || in[i] == QLatin1Char('-'))
        ++i;
    while (i < static_cast<int>(in.size()) && in[i].isDigit())
        ++i;
    if (i < static_cast<int>(in.size()) && in[i] == QLatin1Char('.')) {
        ++i;
        while (i < static_cast<int>(in.size()) && in[i].isDigit())
            ++i;
    }
    if (i < static_cast<int>(in.size()) && (in[i] == QLatin1Char('e') || in[i] == QLatin1Char('E'))) {
        ++i;
        if (i < static_cast<int>(in.size()) && (in[i] == QLatin1Char('+') || in[i] == QLatin1Char('-')))
            ++i;
        while (i < static_cast<int>(in.size()) && in[i].isDigit())
            ++i;
    }
    if (start == i)
        return false;

    bool ok = false;
    const QStringView slice = in.sliced(start, i - start);
    out = QLocale::c().toDouble(slice, &ok);
    return ok;
}

void skip_separators(QStringView in, int& i)
{
    while (i < static_cast<int>(in.size())) {
        const QChar c = in[i];
        if (c.isSpace() || c == QLatin1Char(','))
            ++i;
        else
            break;
    }
}

bool parse_numbers(QStringView in, int& i, int count, double* vals)
{
    for (int k = 0; k < count; ++k) {
        skip_separators(in, i);
        if (!parse_number(in, i, vals[k]))
            return false;
    }
    return true;
}

bool parse_arc_flags(QStringView d, int& i, bool& large_arc, bool& sweep)
{
    skip_separators(d, i);
    if (i >= static_cast<int>(d.size()))
        return false;
    QChar cl = d[i];
    if (cl != QLatin1Char('0') && cl != QLatin1Char('1'))
        return false;
    large_arc = (cl == QLatin1Char('1'));
    ++i;
    skip_separators(d, i);
    if (i >= static_cast<int>(d.size()))
        return false;
    QChar cs = d[i];
    if (cs != QLatin1Char('0') && cs != QLatin1Char('1'))
        return false;
    sweep = (cs == QLatin1Char('1'));
    ++i;
    return true;
}

QPointF cubic_eval(const QPointF& p0, const QPointF& p1, const QPointF& p2, const QPointF& p3, double t)
{
    const double u = 1.0 - t;
    const double tt = t * t;
    const double uu = u * u;
    const double uuu = uu * u;
    const double ttt = tt * t;
    return uuu * p0 + 3 * uu * t * p1 + 3 * u * tt * p2 + ttt * p3;
}

void sample_cubic(const QPointF& p0, const QPointF& p1, const QPointF& p2, const QPointF& p3,
                  double step, std::vector<QPointF>& out)
{
    const double chord =
        QLineF(p0, p1).length() + QLineF(p1, p2).length() + QLineF(p2, p3).length();
    int segments = static_cast<int>(std::ceil(chord / std::max(step, 1e-6)));
    segments = std::clamp(segments, 8, 512);
    for (int k = 1; k <= segments; ++k) {
        const double t = static_cast<double>(k) / static_cast<double>(segments);
        out.push_back(cubic_eval(p0, p1, p2, p3, t));
    }
}

QPointF quad_eval(const QPointF& p0, const QPointF& p1, const QPointF& p2, double t)
{
    const double u = 1.0 - t;
    return u * u * p0 + 2 * u * t * p1 + t * t * p2;
}

void sample_quad(const QPointF& p0, const QPointF& p1, const QPointF& p2, double step,
                 std::vector<QPointF>& out)
{
    const double chord = QLineF(p0, p1).length() + QLineF(p1, p2).length();
    int segments = static_cast<int>(std::ceil(chord / std::max(step, 1e-6)));
    segments = std::clamp(segments, 8, 512);
    for (int k = 1; k <= segments; ++k) {
        const double t = static_cast<double>(k) / static_cast<double>(segments);
        out.push_back(quad_eval(p0, p1, p2, t));
    }
}

static QPointF reflect_pt(double cx, double cy, double px, double py)
{
    return QPointF(2 * cx - px, 2 * cy - py);
}

/// Endpoint-parameterized elliptical arc → polyline on path (WG SVG implementation notes F.6).
static void appendSvgEllipticalArc(QPainterPath& path, double x0, double y0, double rx, double ry,
                                   double phi_deg, bool large_arc, bool sweep_flag, double x1,
                                   double y1)
{
    constexpr double ep = 1e-12;
    if (std::abs(x0 - x1) < ep && std::abs(y0 - y1) < ep)
        return;

    rx = std::abs(rx);
    ry = std::abs(ry);
    if (rx < ep || ry < ep) {
        path.lineTo(x1, y1);
        return;
    }

    const double phi = phi_deg * M_PI / 180.0;
    const double cos_phi = std::cos(phi);
    const double sin_phi = std::sin(phi);

    const double dx = (x0 - x1) / 2.0;
    const double dy = (y0 - y1) / 2.0;
    const double x_ = cos_phi * dx + sin_phi * dy;
    const double y_ = -sin_phi * dx + cos_phi * dy;

    double rx_sq = rx * rx;
    double ry_sq = ry * ry;
    double x__sq = x_ * x_;
    double y__sq = y_ * y_;

    double lambda = x__sq / rx_sq + y__sq / ry_sq;
    if (lambda > 1.0) {
        rx *= std::sqrt(lambda);
        ry *= std::sqrt(lambda);
        rx_sq = rx * rx;
        ry_sq = ry * ry;
    }

    const double sign = (large_arc == sweep_flag) ? -1.0 : 1.0;
    double sq = rx_sq * ry_sq - rx_sq * y__sq - ry_sq * x__sq;
    sq /= rx_sq * y__sq + ry_sq * x__sq;
    sq = std::max(0.0, sq);
    const double coef = sign * std::sqrt(sq);
    const double cx_ = coef * (rx * y_) / ry;
    const double cy_ = coef * (-ry * x_) / rx;

    const double cx = cos_phi * cx_ - sin_phi * cy_ + (x0 + x1) / 2.0;
    const double cy = sin_phi * cx_ + cos_phi * cy_ + (y0 + y1) / 2.0;

    double ux = (x_ - cx_) / rx;
    double uy = (y_ - cy_) / ry;
    double vx = (-x_ - cx_) / rx;
    double vy = (-y_ - cy_) / ry;

    double n = std::hypot(ux, uy);
    if (n < ep)
        return;
    ux /= n;
    uy /= n;
    n = std::hypot(vx, vy);
    if (n < ep)
        return;
    vx /= n;
    vy /= n;

    double theta1 = std::atan2(uy, ux);
    double theta2 = std::atan2(vy, vx);
    double delta = theta2 - theta1;

    const double two_pi = 2.0 * M_PI;
    if (!sweep_flag && delta > 0)
        delta -= two_pi;
    else if (sweep_flag && delta < 0)
        delta += two_pi;

    if (large_arc) {
        if (std::abs(delta) <= M_PI)
            delta += (delta > 0 ? -two_pi : two_pi);
    } else {
        if (std::abs(delta) >= M_PI)
            delta += (delta > 0 ? -two_pi : two_pi);
    }

    const int seg =
        std::clamp(static_cast<int>(std::ceil(std::abs(delta) / (M_PI / 16))), 8, 512);
    for (int k = 1; k <= seg; ++k) {
        const double t = static_cast<double>(k) / static_cast<double>(seg);
        const double ang = theta1 + t * delta;
        const double ct = std::cos(ang);
        const double st = std::sin(ang);
        const double ex = cx + rx * ct * cos_phi - ry * st * sin_phi;
        const double ey = cy + rx * ct * sin_phi + ry * st * cos_phi;
        path.lineTo(ex, ey);
    }
}

} // namespace

bool append_svg_path_d(QPainterPath& path, QStringView d)
{
    int i = 0;
    double cx = 0.0;
    double cy = 0.0;
    double sx = 0.0;
    double sy = 0.0;
    QChar cmd;

    bool has_cubic_cp2 = false;
    double last_c2x = 0, last_c2y = 0;
    bool has_quad_cp = false;
    double last_qx = 0, last_qy = 0;

    auto reset_smooth = [&]() {
        has_cubic_cp2 = false;
        has_quad_cp = false;
    };

    while (i < static_cast<int>(d.size())) {
        skip_separators(d, i);
        if (i >= static_cast<int>(d.size()))
            break;

        if (d[i].isLetter()) {
            cmd = d[i];
            ++i;
        }

        const bool rel = cmd.isLower();
        const ushort uc = cmd.toUpper().unicode();

        if (uc == 'M') {
            double xy[2]{};
            if (!parse_numbers(d, i, 2, xy))
                return false;
            if (rel) {
                cx += xy[0];
                cy += xy[1];
            } else {
                cx = xy[0];
                cy = xy[1];
            }
            sx = cx;
            sy = cy;
            path.moveTo(cx, cy);
            reset_smooth();
            cmd = rel ? QLatin1Char('l') : QLatin1Char('L');

            while (true) {
                skip_separators(d, i);
                const int save = i;
                double extra[2]{};
                if (!parse_numbers(d, i, 2, extra)) {
                    i = save;
                    break;
                }
                double nx = extra[0];
                double ny = extra[1];
                if (rel) {
                    cx += nx;
                    cy += ny;
                } else {
                    cx = nx;
                    cy = ny;
                }
                path.lineTo(cx, cy);
            }
            reset_smooth();
            continue;
        }

        if (uc == 'L') {
            double xy[2]{};
            if (!parse_numbers(d, i, 2, xy))
                return false;
            if (rel) {
                cx += xy[0];
                cy += xy[1];
            } else {
                cx = xy[0];
                cy = xy[1];
            }
            path.lineTo(cx, cy);
            reset_smooth();
            continue;
        }

        if (uc == 'H') {
            double x{};
            if (!parse_numbers(d, i, 1, &x))
                return false;
            if (rel)
                cx += x;
            else
                cx = x;
            path.lineTo(cx, cy);
            reset_smooth();
            continue;
        }

        if (uc == 'V') {
            double y{};
            if (!parse_numbers(d, i, 1, &y))
                return false;
            if (rel)
                cy += y;
            else
                cy = y;
            path.lineTo(cx, cy);
            reset_smooth();
            continue;
        }

        if (uc == 'C') {
            double v[6]{};
            if (!parse_numbers(d, i, 6, v))
                return false;
            QPointF c1(v[0], v[1]);
            QPointF c2(v[2], v[3]);
            QPointF end(v[4], v[5]);
            if (rel) {
                c1 += QPointF(cx, cy);
                c2 += QPointF(cx, cy);
                end += QPointF(cx, cy);
            }
            path.cubicTo(c1, c2, end);
            cx = end.x();
            cy = end.y();
            last_c2x = c2.x();
            last_c2y = c2.y();
            has_cubic_cp2 = true;
            has_quad_cp = false;
            continue;
        }

        if (uc == 'S') {
            double v[4]{};
            if (!parse_numbers(d, i, 4, v))
                return false;
            QPointF c1 = has_cubic_cp2 ? reflect_pt(cx, cy, last_c2x, last_c2y) : QPointF(cx, cy);
            QPointF c2(v[0], v[1]);
            QPointF end(v[2], v[3]);
            if (rel) {
                c2 += QPointF(cx, cy);
                end += QPointF(cx, cy);
            }
            path.cubicTo(c1, c2, end);
            cx = end.x();
            cy = end.y();
            last_c2x = c2.x();
            last_c2y = c2.y();
            has_cubic_cp2 = true;
            has_quad_cp = false;
            continue;
        }

        if (uc == 'Q') {
            double v[4]{};
            if (!parse_numbers(d, i, 4, v))
                return false;
            QPointF c1(v[0], v[1]);
            QPointF end(v[2], v[3]);
            if (rel) {
                c1 += QPointF(cx, cy);
                end += QPointF(cx, cy);
            }
            path.quadTo(c1, end);
            cx = end.x();
            cy = end.y();
            last_qx = c1.x();
            last_qy = c1.y();
            has_quad_cp = true;
            has_cubic_cp2 = false;
            continue;
        }

        if (uc == 'T') {
            double xy[2]{};
            if (!parse_numbers(d, i, 2, xy))
                return false;
            QPointF c1 = has_quad_cp ? reflect_pt(cx, cy, last_qx, last_qy) : QPointF(cx, cy);
            QPointF end(xy[0], xy[1]);
            if (rel)
                end += QPointF(cx, cy);
            path.quadTo(c1, end);
            cx = end.x();
            cy = end.y();
            last_qx = c1.x();
            last_qy = c1.y();
            has_quad_cp = true;
            has_cubic_cp2 = false;
            continue;
        }

        if (uc == 'A') {
            while (true) {
                double ar[3]{};
                if (!parse_numbers(d, i, 3, ar))
                    return false;
                bool fa = false;
                bool fs = false;
                if (!parse_arc_flags(d, i, fa, fs))
                    return false;
                double xy[2]{};
                if (!parse_numbers(d, i, 2, xy))
                    return false;
                double ex = xy[0];
                double ey = xy[1];
                if (rel) {
                    ex += cx;
                    ey += cy;
                }
                appendSvgEllipticalArc(path, cx, cy, ar[0], ar[1], ar[2], fa, fs, ex, ey);
                cx = ex;
                cy = ey;
                reset_smooth();

                skip_separators(d, i);
                if (i >= static_cast<int>(d.size()) || d[i].isLetter())
                    break;
            }
            continue;
        }

        if (uc == 'Z') {
            path.closeSubpath();
            cx = sx;
            cy = sy;
            reset_smooth();
            continue;
        }

        return false;
    }

    return true;
}

MoveCutPaths splitMoveCutPaths(const QPainterPath& path)
{
    MoveCutPaths out;
    out.cut = path;

    QPointF current;
    bool have_current = false;

    for (int i = 0; i < path.elementCount(); ++i) {
        const QPainterPath::Element e = path.elementAt(i);
        const QPointF p(e.x, e.y);
        if (e.type == QPainterPath::MoveToElement) {
            if (have_current) {
                if (out.move.isEmpty())
                    out.move.moveTo(current);
                out.move.lineTo(p);
            }
            current = p;
            have_current = true;
        } else {
            current = p;
            have_current = true;
        }
    }

    return out;
}

std::vector<std::vector<QPointF>> path_to_polylines(const QPainterPath& path, double flatness_step)
{
    std::vector<std::vector<QPointF>> polys;
    std::vector<QPointF> cur;
    QPointF pos;

    auto flush = [&]() {
        if (!cur.empty()) {
            polys.push_back(std::move(cur));
            cur.clear();
        }
    };

    int i = 0;
    while (i < path.elementCount()) {
        const QPainterPath::Element e = path.elementAt(i);
        switch (e.type) {
        case QPainterPath::MoveToElement:
            flush();
            pos = QPointF(e.x, e.y);
            cur.push_back(pos);
            ++i;
            break;
        case QPainterPath::LineToElement:
            pos = QPointF(e.x, e.y);
            cur.push_back(pos);
            ++i;
            break;
        case QPainterPath::CurveToElement: {
            const int n = path.elementCount();
            if (i + 1 >= n) {
                ++i;
                break;
            }
            const auto e1 = path.elementAt(i + 1);
            if (e1.type != QPainterPath::CurveToDataElement) {
                ++i;
                break;
            }
            const QPointF c1(e.x, e.y);
            const bool cubic =
                (i + 2 < n) && path.elementAt(i + 2).type == QPainterPath::CurveToDataElement;
            if (cubic) {
                const auto e2 = path.elementAt(i + 2);
                const QPointF c2(e1.x, e1.y);
                const QPointF end(e2.x, e2.y);
                sample_cubic(pos, c1, c2, end, flatness_step, cur);
                pos = end;
                i += 3;
            } else {
                const QPointF end(e1.x, e1.y);
                sample_quad(pos, c1, end, flatness_step, cur);
                pos = end;
                i += 2;
            }
            break;
        }
        default:
            ++i;
            break;
        }
    }
    flush();
    return polys;
}

bool append_paths_from_svg_xml(const QString& xml, QPainterPath& out, QString* error_message)
{
    return loadSvgPainterPath(xml, out, error_message);
}

} // namespace inkcut
