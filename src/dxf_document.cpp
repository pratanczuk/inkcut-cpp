// SPDX-License-Identifier: GPL-3.0-or-later

#include "dxf_document.hpp"

#include <QLocale>
#include <QPainterPath>
#include <QRegularExpression>
#include <QtEndian>

#include <cstring>
#include <cmath>

#include "i18n.hpp"

namespace inkcut {

namespace {

struct Pair {
    int code = 0;
    QString value;
};

double toD(QStringView s)
{
    bool ok = false;
    const double v = QLocale::c().toDouble(QString::fromUtf16(s.utf16(), s.size()), &ok);
    return ok ? v : 0.0;
}

QVector<Pair> ascii_pairs_from_raw(const QByteArray& raw)
{
    const QString text = QString::fromLatin1(raw);
    const QStringList lines =
        text.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);
    QVector<Pair> pairs;
    pairs.reserve(lines.size() / 2);
    for (int i = 0; i + 1 < lines.size(); ++i) {
        bool ok = false;
        const int code = lines[i].trimmed().toInt(&ok);
        if (!ok)
            continue;
        ++i;
        pairs.push_back({ code, lines[i].trimmed() });
    }
    return pairs;
}

static qint16 read_le_i16(const QByteArray& b, int pos)
{
    quint16 u = quint16(uchar(b[pos])) | (quint16(uchar(b[pos + 1])) << 8);
    qint16 v;
    std::memcpy(&v, &u, 2);
    return qint16(qFromLittleEndian(quint16(u)));
}

static qint32 read_le_i32(const QByteArray& b, int pos)
{
    quint32 u = quint32(uchar(b[pos])) | (quint32(uchar(b[pos + 1])) << 8)
        | (quint32(uchar(b[pos + 2])) << 16) | (quint32(uchar(b[pos + 3])) << 24);
    return qint32(qFromLittleEndian(u));
}

static double read_le_f64(const QByteArray& b, int pos)
{
    quint64 u = 0;
    for (int k = 0; k < 8; ++k)
        u |= quint64(uchar(b[pos + k])) << (8 * k);
    u = qFromLittleEndian(u);
    double d;
    std::memcpy(&d, &u, 8);
    return d;
}

static qint64 read_le_i64(const QByteArray& b, int pos)
{
    quint64 u = 0;
    for (int k = 0; k < 8; ++k)
        u |= quint64(uchar(b[pos + k])) << (8 * k);
    u = qFromLittleEndian(u);
    qint64 v;
    std::memcpy(&v, &u, 8);
    return v;
}

static QString read_binary_string(const QByteArray& raw, int& pos)
{
    const int z = raw.indexOf('\0', pos);
    if (z < 0) {
        pos = raw.size();
        return {};
    }
    const QString s = QString::fromLatin1(raw.constData() + pos, z - pos);
    pos = z + 1;
    return s;
}

/// Najlepszy wysiłek dla DXF binarnego (AutoCAD): zero-terminated stringi / double / int16 / int32.
QVector<Pair> binary_pairs_best_effort(const QByteArray& raw)
{
    static const QByteArray sentinel = "AutoCAD Binary DXF\r\n\x1a\x00";
    if (!raw.startsWith(sentinel))
        return {};

    QVector<Pair> pairs;
    int pos = sentinel.size();

    while (pos + 2 <= raw.size()) {
        const qint16 code16 = read_le_i16(raw, pos);
        pos += 2;
        const int code = int(code16);

        QString val;

        /* Typ wartości wg typowych zakresów kodów grup DXF */
        if ((code >= 0 && code <= 9) || (code >= 300 && code <= 309) || (code >= 100 && code <= 102)
            || code == 105 || (code >= 350 && code <= 369) || (code >= 390 && code <= 399)) {
            val = read_binary_string(raw, pos);
        } else if ((code >= 10 && code <= 59) || (code >= 110 && code <= 149) || (code >= 210 && code <= 239)
                   || (code >= 140 && code <= 149)) {
            if (pos + 8 > raw.size())
                break;
            val = QString::number(read_le_f64(raw, pos), 'g', 17);
            pos += 8;
        } else if ((code >= 60 && code <= 79) || (code >= 170 && code <= 179)) {
            if (pos + 2 > raw.size())
                break;
            val = QString::number(int(read_le_i16(raw, pos)));
            pos += 2;
        } else if ((code >= 90 && code <= 99) || (code >= 370 && code <= 389)) {
            if (pos + 4 > raw.size())
                break;
            val = QString::number(read_le_i32(raw, pos));
            pos += 4;
        } else if (code >= 160 && code <= 169) {
            if (pos + 8 > raw.size())
                break;
            val = QString::number(read_le_i64(raw, pos));
            pos += 8;
        } else {
            break;
        }

        pairs.push_back({ code, val });
    }

    return pairs;
}

QVector<Pair> decode_pairs_from_bytes(const QByteArray& raw)
{
    QVector<Pair> bin = binary_pairs_best_effort(raw);
    if (!bin.isEmpty())
        return bin;
    return ascii_pairs_from_raw(raw);
}

void append_bulge_arc(QPainterPath& path, const QPointF& p0, const QPointF& p1, double bulge)
{
    if (qAbs(bulge) < 1e-12) {
        path.lineTo(p1);
        return;
    }

    const double theta = 4.0 * std::atan(bulge);
    const double dx = p1.x() - p0.x();
    const double dy = p1.y() - p0.y();
    const double L = std::hypot(dx, dy);
    if (L < 1e-15)
        return;

    const double sin_h = std::sin(theta * 0.5);
    if (qAbs(sin_h) < 1e-14) {
        path.lineTo(p1);
        return;
    }

    const double r = (L * 0.5) / sin_h;
    QPointF n(-dy / L, dx / L);
    if (bulge < 0)
        n = QPointF(-n.x(), -n.y());

    const QPointF mid((p0.x() + p1.x()) * 0.5, (p0.y() + p1.y()) * 0.5);
    const double h = r * std::cos(theta * 0.5);
    const QPointF c(mid.x() + n.x() * h, mid.y() + n.y() * h);

    double a0 = std::atan2(p0.y() - c.y(), p0.x() - c.x());
    double a1 = std::atan2(p1.y() - c.y(), p1.x() - c.x());

    auto wrap_diff = [](double d) {
        while (d <= -M_PI)
            d += 2 * M_PI;
        while (d > M_PI)
            d -= 2 * M_PI;
        return d;
    };

    double sweep = wrap_diff(a1 - a0);
    if (theta > 0 && sweep < 0)
        sweep += 2 * M_PI;
    if (theta < 0 && sweep > 0)
        sweep -= 2 * M_PI;

    const int segs =
        qBound(4, static_cast<int>(std::ceil(std::fabs(sweep) / (M_PI / 16))), 256);
    for (int i = 1; i <= segs; ++i) {
        const double t = static_cast<double>(i) / segs;
        const double a = a0 + sweep * t;
        path.lineTo(c.x() + r * std::cos(a), c.y() + r * std::sin(a));
    }
}

void emit_poly_vertices(QPainterPath& out, const QVector<QPointF>& verts,
                        const QVector<double>& bulges, bool closed)
{
    const int n = static_cast<int>(verts.size());
    if (n < 2)
        return;

    out.moveTo(verts[0]);
    for (int i = 0; i < n - 1; ++i) {
        const double b = i < bulges.size() ? bulges[i] : 0.0;
        append_bulge_arc(out, verts[i], verts[i + 1], b);
    }
    if (closed) {
        const double b = (n - 1 < bulges.size()) ? bulges[n - 1] : 0.0;
        append_bulge_arc(out, verts[n - 1], verts[0], b);
    }
}

using BlockBodies = QHash<QString, QVector<Pair>>;
using BlockBases = QHash<QString, QPointF>;

size_t parse_line(const QVector<Pair>& p, size_t idx, QPainterPath& out)
{
    double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    while (idx < p.size() && p[idx].code != 0) {
        if (p[idx].code == 10)
            x1 = toD(QStringView(p[idx].value));
        else if (p[idx].code == 20)
            y1 = toD(QStringView(p[idx].value));
        else if (p[idx].code == 11)
            x2 = toD(QStringView(p[idx].value));
        else if (p[idx].code == 21)
            y2 = toD(QStringView(p[idx].value));
        ++idx;
    }
    out.moveTo(x1, y1);
    out.lineTo(x2, y2);
    return idx;
}

size_t parse_circle(const QVector<Pair>& p, size_t idx, QPainterPath& out)
{
    double cx = 0, cy = 0, r = 0;
    while (idx < p.size() && p[idx].code != 0) {
        if (p[idx].code == 10)
            cx = toD(QStringView(p[idx].value));
        else if (p[idx].code == 20)
            cy = toD(QStringView(p[idx].value));
        else if (p[idx].code == 40)
            r = toD(QStringView(p[idx].value));
        ++idx;
    }
    if (r > 0)
        out.addEllipse(QPointF(cx, cy), r, r);
    return idx;
}

size_t parse_arc(const QVector<Pair>& p, size_t idx, QPainterPath& out)
{
    double cx = 0, cy = 0, r = 0, a0 = 0, a1 = 0;
    while (idx < p.size() && p[idx].code != 0) {
        if (p[idx].code == 10)
            cx = toD(QStringView(p[idx].value));
        else if (p[idx].code == 20)
            cy = toD(QStringView(p[idx].value));
        else if (p[idx].code == 40)
            r = toD(QStringView(p[idx].value));
        else if (p[idx].code == 50)
            a0 = toD(QStringView(p[idx].value)) * M_PI / 180.0;
        else if (p[idx].code == 51)
            a1 = toD(QStringView(p[idx].value)) * M_PI / 180.0;
        ++idx;
    }
    if (r <= 0)
        return idx;

    double sweep = a1 - a0;
    while (sweep < 0)
        sweep += 2 * M_PI;
    while (sweep > 2 * M_PI)
        sweep -= 2 * M_PI;

    const int segs =
        qBound(4, static_cast<int>(std::ceil(sweep / (M_PI / 16))), 256);
    const QPointF start(cx + r * std::cos(a0), cy + r * std::sin(a0));
    out.moveTo(start);
    for (int i = 1; i <= segs; ++i) {
        const double t = static_cast<double>(i) / segs;
        const double ang = a0 + sweep * t;
        out.lineTo(cx + r * std::cos(ang), cy + r * std::sin(ang));
    }
    return idx;
}

size_t parse_lwpolyline(const QVector<Pair>& p, size_t idx, QPainterPath& out)
{
    QVector<QPointF> verts;
    QVector<double> bulges;
    int flags = 0;

    double pending_bulge = 0;
    double vx = 0;
    bool have_x = false;

    while (idx < p.size() && p[idx].code != 0) {
        const Pair& pr = p[idx];
        if (pr.code == 10) {
            vx = toD(QStringView(pr.value));
            have_x = true;
        } else if (pr.code == 20 && have_x) {
            if (!verts.isEmpty())
                bulges.push_back(pending_bulge);
            pending_bulge = 0;
            verts.push_back(QPointF(vx, toD(QStringView(pr.value))));
            have_x = false;
        } else if (pr.code == 42)
            pending_bulge = toD(QStringView(pr.value));
        else if (pr.code == 70)
            flags = pr.value.toInt();
        ++idx;
    }

    const bool closed = (flags & 1) != 0;
    if (closed && verts.size() >= 2)
        bulges.push_back(pending_bulge);

    emit_poly_vertices(out, verts, bulges, closed);
    return idx;
}

size_t parse_polyline(const QVector<Pair>& p, size_t idx, QPainterPath& out)
{
    int flags = 0;
    QVector<QPointF> verts;
    QVector<double> bulges;

    while (idx < p.size()) {
        const Pair& pr = p[idx];
        if (pr.code == 0) {
            const QString u = pr.value.toUpper();
            if (u == QLatin1String("VERTEX")) {
                ++idx;
                double x = 0, y = 0, b = 0;
                while (idx < p.size() && p[idx].code != 0) {
                    if (p[idx].code == 10)
                        x = toD(QStringView(p[idx].value));
                    else if (p[idx].code == 20)
                        y = toD(QStringView(p[idx].value));
                    else if (p[idx].code == 42)
                        b = toD(QStringView(p[idx].value));
                    ++idx;
                }
                verts.push_back(QPointF(x, y));
                bulges.push_back(b);
                continue;
            }
            if (u == QLatin1String("SEQEND")) {
                ++idx;
                break;
            }
            break;
        }
        if (pr.code == 70)
            flags = pr.value.toInt();
        ++idx;
    }

    while (bulges.size() < verts.size())
        bulges.push_back(0);

    const bool closed = (flags & 1) != 0;
    emit_poly_vertices(out, verts, bulges, closed);
    return idx;
}

size_t parse_spline(const QVector<Pair>& p, size_t idx, QPainterPath& out)
{
    QVector<QPointF> fit;
    QVector<QPointF> ctrl;
    double px = 0;
    double py = 0;
    enum Pending : quint8 { None, Await21Fit, Await20Ctrl } pend = None;

    while (idx < p.size() && p[idx].code != 0) {
        const Pair& pr = p[idx];
        if (pr.code == 11) {
            px = toD(QStringView(pr.value));
            pend = Await21Fit;
        } else if (pr.code == 21 && pend == Await21Fit) {
            py = toD(QStringView(pr.value));
            fit.push_back(QPointF(px, py));
            pend = None;
        } else if (pr.code == 10) {
            px = toD(QStringView(pr.value));
            pend = Await20Ctrl;
        } else if (pr.code == 20 && pend == Await20Ctrl) {
            py = toD(QStringView(pr.value));
            ctrl.push_back(QPointF(px, py));
            pend = None;
        } else if (pr.code == 21 || pr.code == 20)
            pend = None;

        ++idx;
    }

    const QVector<QPointF>* src = !fit.isEmpty() ? &fit : &ctrl;
    if (src->size() >= 2) {
        out.moveTo((*src)[0]);
        for (int i = 1; i < src->size(); ++i)
            out.lineTo((*src)[i]);
    }
    return idx;
}

size_t parse_ellipse(const QVector<Pair>& p, size_t idx, QPainterPath& out)
{
    double cx = 0, cy = 0;
    double mx = 1, my = 0;
    double ratio = 1;
    double p0 = 0, p1 = 2 * M_PI;

    while (idx < p.size() && p[idx].code != 0) {
        const Pair& pr = p[idx];
        if (pr.code == 10)
            cx = toD(QStringView(pr.value));
        else if (pr.code == 20)
            cy = toD(QStringView(pr.value));
        else if (pr.code == 11)
            mx = toD(QStringView(pr.value));
        else if (pr.code == 21)
            my = toD(QStringView(pr.value));
        else if (pr.code == 40)
            ratio = toD(QStringView(pr.value));
        else if (pr.code == 41)
            p0 = toD(QStringView(pr.value));
        else if (pr.code == 42)
            p1 = toD(QStringView(pr.value));
        ++idx;
    }

    const double sma = std::hypot(mx, my);
    if (sma < 1e-15)
        return idx;
    const double smb = sma * ratio;
    QPointF u(mx / sma, my / sma);
    QPointF v(-u.y(), u.x());

    double sweep = p1 - p0;
    while (sweep < 0)
        sweep += 2 * M_PI;
    while (sweep > 2 * M_PI)
        sweep -= 2 * M_PI;

    const int segs =
        qBound(8, static_cast<int>(std::ceil(sweep / (M_PI / 24))), 512);
    const QPointF start(cx + std::cos(p0) * sma * u.x() + std::sin(p0) * smb * v.x(),
                        cy + std::cos(p0) * sma * u.y() + std::sin(p0) * smb * v.y());
    out.moveTo(start);
    for (int i = 1; i <= segs; ++i) {
        const double t = static_cast<double>(i) / segs;
        const double ang = p0 + sweep * t;
        const QPointF pt(cx + std::cos(ang) * sma * u.x() + std::sin(ang) * smb * v.x(),
                         cy + std::cos(ang) * sma * u.y() + std::sin(ang) * smb * v.y());
        out.lineTo(pt);
    }
    return idx;
}

size_t skip_entity(const QVector<Pair>& p, size_t idx)
{
    while (idx < p.size() && p[idx].code != 0)
        ++idx;
    return idx;
}

void extract_block_definitions(const QVector<Pair>& pairs, BlockBodies& bodies, BlockBases& bases)
{
    size_t idx = 0;
    while (idx < pairs.size()) {
        if (pairs[idx].code == 0 && pairs[idx].value.compare(QLatin1String("SECTION"), Qt::CaseInsensitive)
                                    == 0) {
            ++idx;
            if (idx < pairs.size() && pairs[idx].code == 2
                && pairs[idx].value.compare(QLatin1String("BLOCKS"), Qt::CaseInsensitive) == 0) {
                ++idx;
                while (idx < pairs.size()) {
                    const Pair& pr = pairs[idx];
                    if (pr.code == 0 && pr.value.compare(QLatin1String("ENDSEC"), Qt::CaseInsensitive) == 0) {
                        ++idx;
                        break;
                    }
                    if (pr.code == 0 && pr.value.compare(QLatin1String("BLOCK"), Qt::CaseInsensitive) == 0) {
                        ++idx;
                        QString name;
                        QPointF base(0, 0);
                        while (idx < pairs.size() && pairs[idx].code != 0) {
                            if (pairs[idx].code == 2)
                                name = pairs[idx].value;
                            else if (pairs[idx].code == 10)
                                base.setX(toD(QStringView(pairs[idx].value)));
                            else if (pairs[idx].code == 20)
                                base.setY(toD(QStringView(pairs[idx].value)));
                            ++idx;
                        }
                        QVector<Pair> inner;
                        while (idx < pairs.size()) {
                            if (pairs[idx].code == 0
                                && pairs[idx].value.compare(QLatin1String("ENDBLK"), Qt::CaseInsensitive)
                                       == 0) {
                                ++idx;
                                break;
                            }
                            inner.push_back(pairs[idx]);
                            ++idx;
                        }
                        if (!name.isEmpty()) {
                            bodies.insert(name, inner);
                            bases.insert(name, base);
                        }
                        continue;
                    }
                    ++idx;
                }
                continue;
            }
            ++idx;
            continue;
        }
        ++idx;
    }
}

size_t parse_insert(const QVector<Pair>& p, size_t idx, QPainterPath& out, const BlockBodies& bodies,
                    const BlockBases& bases, int depth, QString* err);

size_t dispatch_entity(const QVector<Pair>& seq, size_t idx, QPainterPath& out,
                       const BlockBodies& bodies, const BlockBases& bases, int depth, QString* err);

size_t parse_sequence_entities(const QVector<Pair>& seq, size_t start, QPainterPath& out,
                               const BlockBodies& bodies, const BlockBases& bases, int depth,
                               QString* err)
{
    size_t idx = start;
    while (idx < seq.size()) {
        const Pair& pr = seq[idx];
        if (pr.code != 0)
            ++idx;
        else
            idx = dispatch_entity(seq, idx, out, bodies, bases, depth, err);
    }
    return idx;
}

size_t dispatch_entity(const QVector<Pair>& seq, size_t idx, QPainterPath& out,
                       const BlockBodies& bodies, const BlockBases& bases, int depth, QString* err)
{
    const QString et = seq[idx].value.toUpper();
    ++idx;

    if (et == QLatin1String("LINE"))
        return parse_line(seq, idx, out);
    if (et == QLatin1String("CIRCLE"))
        return parse_circle(seq, idx, out);
    if (et == QLatin1String("ARC"))
        return parse_arc(seq, idx, out);
    if (et == QLatin1String("LWPOLYLINE"))
        return parse_lwpolyline(seq, idx, out);
    if (et == QLatin1String("POLYLINE"))
        return parse_polyline(seq, idx, out);
    if (et == QLatin1String("SPLINE"))
        return parse_spline(seq, idx, out);
    if (et == QLatin1String("ELLIPSE"))
        return parse_ellipse(seq, idx, out);
    if (et == QLatin1String("INSERT"))
        return parse_insert(seq, idx, out, bodies, bases, depth, err);

    return skip_entity(seq, idx);
}

QPainterPath instantiate_block_local(const QString& block_name, const BlockBodies& bodies,
                                     const BlockBases& bases, int depth, QString* err);

size_t parse_insert(const QVector<Pair>& p, size_t idx, QPainterPath& out, const BlockBodies& bodies,
                    const BlockBases& bases, int depth, QString* err)
{
    QString name;
    double ix = 0, iy = 0, sx = 1, sy = 1, rot = 0;

    while (idx < p.size() && p[idx].code != 0) {
        if (p[idx].code == 2)
            name = p[idx].value;
        else if (p[idx].code == 10)
            ix = toD(QStringView(p[idx].value));
        else if (p[idx].code == 20)
            iy = toD(QStringView(p[idx].value));
        else if (p[idx].code == 41)
            sx = toD(QStringView(p[idx].value));
        else if (p[idx].code == 42)
            sy = toD(QStringView(p[idx].value));
        else if (p[idx].code == 50)
            rot = toD(QStringView(p[idx].value));
        ++idx;
    }

    if (depth > 96) {
        if (err)
            *err = trInk("DXF: zbyt głębokie INSERT/bloki.");
        return idx;
    }

    if (name.isEmpty())
        return idx;

    QPainterPath block_local = instantiate_block_local(name, bodies, bases, depth + 1, err);
    if (block_local.isEmpty())
        return idx;

    const QPointF base = bases.value(name);
    QTransform xf;
    xf.translate(ix, iy);
    xf.rotate(rot);
    xf.scale(sx, sy);
    xf.translate(-base.x(), -base.y());

    out.addPath(xf.map(block_local));
    return idx;
}

QPainterPath instantiate_block_local(const QString& block_name, const BlockBodies& bodies,
                                     const BlockBases& bases, int depth, QString* err)
{
    QPainterPath acc;
    const QVector<Pair> seq = bodies.value(block_name);
    if (seq.isEmpty())
        return acc;

    parse_sequence_entities(seq, 0, acc, bodies, bases, depth, err);
    return acc;
}

bool append_entities_section(const QVector<Pair>& pairs, QPainterPath& out, QString* error_message)
{
    BlockBodies bodies;
    BlockBases bases;
    extract_block_definitions(pairs, bodies, bases);

    size_t idx = 0;
    bool any = false;
    while (idx < pairs.size()) {
        const Pair& pr = pairs[idx];
        if (pr.code == 0 && pr.value.compare(QLatin1String("SECTION"), Qt::CaseInsensitive) == 0) {
            ++idx;
            if (idx < pairs.size() && pairs[idx].code == 2
                && pairs[idx].value.compare(QLatin1String("ENTITIES"), Qt::CaseInsensitive) == 0) {
                ++idx;
                while (idx < pairs.size()) {
                    if (pairs[idx].code == 0
                        && pairs[idx].value.compare(QLatin1String("ENDSEC"), Qt::CaseInsensitive) == 0) {
                        ++idx;
                        break;
                    }
                    if (pairs[idx].code != 0) {
                        ++idx;
                        continue;
                    }
                    const QString et = pairs[idx].value.toUpper();
                    ++idx;
                    const int elems_before = out.elementCount();
                    if (et == QLatin1String("LINE"))
                        idx = parse_line(pairs, idx, out);
                    else if (et == QLatin1String("CIRCLE"))
                        idx = parse_circle(pairs, idx, out);
                    else if (et == QLatin1String("ARC"))
                        idx = parse_arc(pairs, idx, out);
                    else if (et == QLatin1String("LWPOLYLINE"))
                        idx = parse_lwpolyline(pairs, idx, out);
                    else if (et == QLatin1String("POLYLINE"))
                        idx = parse_polyline(pairs, idx, out);
                    else if (et == QLatin1String("SPLINE"))
                        idx = parse_spline(pairs, idx, out);
                    else if (et == QLatin1String("ELLIPSE"))
                        idx = parse_ellipse(pairs, idx, out);
                    else if (et == QLatin1String("INSERT"))
                        idx = parse_insert(pairs, idx, out, bodies, bases, 0, error_message);
                    else
                        idx = skip_entity(pairs, idx);

                    if (out.elementCount() > elems_before)
                        any = true;
                }
                continue;
            }
            ++idx;
            continue;
        }
        ++idx;
    }

    if (!any && out.isEmpty()) {
        if (error_message)
            *error_message =
                QStringLiteral("DXF: brak geometrii w ENTITIES (obsługa: LINE, ARC, CIRCLE, "
                               "LWPOLYLINE, POLYLINE, SPLINE, ELLIPSE, INSERT z blokami).");
        return false;
    }

    return true;
}

} // namespace

bool loadDxfPainterPathFromBytes(const QByteArray& raw, QPainterPath& out, QString* error_message)
{
    out = QPainterPath();

    if (raw.isEmpty()) {
        if (error_message)
            *error_message = trInk("DXF: pusty plik.");
        return false;
    }

    const QVector<Pair> pairs = decode_pairs_from_bytes(raw);
    if (pairs.isEmpty()) {
        if (error_message)
            *error_message =
                trInk("DXF: nie można zdekodować par grup (ASCII lub binarny). Zapisz jako "
                      "ASCII DXF lub sprawdź integrę pliku.");
        return false;
    }

    if (!append_entities_section(pairs, out, error_message))
        return false;

    return true;
}

} // namespace inkcut
