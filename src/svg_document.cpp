// SPDX-License-Identifier: GPL-3.0-or-later

#include "svg_document.hpp"

#include "bitmap_trace.hpp"
#include "svg_path.hpp"

#include <QDomDocument>
#include <QImageReader>
#include <QUrl>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontMetricsF>
#include <QHash>
#include <QLocale>
#include <QPainterPathStroker>
#include <QRegularExpression>
#include <QRectF>
#include <QUrl>
#include <optional>
#include <algorithm>
#include <cmath>

namespace inkcut {

namespace {

/// Inkcut/QtSvg: 90 user units per inch (upstream inkcut/core/svg.py). Paths are stored in mm.
constexpr double kMmPerPlotUnit = 25.4 / 90.0;

double parseUnit(QStringView val, double fallback = 0.0)
{
    val = val.trimmed();
    if (val.isEmpty())
        return fallback;

    static const QHash<QString, double> kUu{
        { QStringLiteral("in"), 90.0 },
        { QStringLiteral("pt"), 1.25 },
        { QStringLiteral("px"), 1.0 },
        { QStringLiteral("mm"), 3.5433070866 },
        { QStringLiteral("cm"), 35.433070866 },
        { QStringLiteral("m"), 3543.3070866 },
        { QStringLiteral("km"), 3543307.0866 },
        { QStringLiteral("pc"), 15.0 },
        { QStringLiteral("yd"), 3240.0 },
        { QStringLiteral("ft"), 1080.0 },
    };

    QString num_part;
    QString unit_part;
    for (int i = 0; i < val.size(); ++i) {
        const QChar c = val[i];
        if ((c.isDigit() || c == QLatin1Char('.') || c == QLatin1Char('-') || c == QLatin1Char('+')
             || c == QLatin1Char('e') || c == QLatin1Char('E'))
            && unit_part.isEmpty())
            num_part.append(c);
        else if (!c.isSpace())
            unit_part.append(c);
    }
    bool ok = false;
    const double n = QLocale::c().toDouble(num_part, &ok);
    if (!ok)
        return fallback;
    unit_part = unit_part.trimmed().toLower();
    if (unit_part.isEmpty())
        return n;
    return n * kUu.value(unit_part, 1.0);
}

bool parse_numbers_csv(QStringView args, std::vector<double>& out)
{
    QString chunk = QString::fromUtf16(args.utf16(), args.size()).replace(QLatin1Char(','), QLatin1Char(' '));
    const QStringList parts = chunk.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString& p : parts) {
        bool ok = false;
        const double v = QLocale::c().toDouble(p, &ok);
        if (!ok)
            return false;
        out.push_back(v);
    }
    return true;
}

QTransform parseTransformChain(QStringView trans)
{
    QTransform result;
    trans = trans.trimmed();
    if (trans.isEmpty())
        return result;

    static const QStringList names = { QStringLiteral("translate"), QStringLiteral("scale"),
                                       QStringLiteral("rotate"),    QStringLiteral("skewX"),
                                       QStringLiteral("skewY"),     QStringLiteral("matrix") };

    QStringView t = trans;
    QString matched_name;
    int idx = -1;
    for (int i = 0; i < names.size(); ++i) {
        const QString& n = names[i];
        if (t.startsWith(QStringView(n), Qt::CaseInsensitive)) {
            matched_name = n;
            idx = static_cast<int>(n.size());
            break;
        }
    }
    if (idx < 0 || idx >= t.size() || t[idx].unicode() != '(')
        return result;

    int depth = 0;
    int end = idx;
    for (; end < t.size(); ++end) {
        if (t[end] == QLatin1Char('('))
            ++depth;
        else if (t[end] == QLatin1Char(')')) {
            --depth;
            if (depth == 0)
                break;
        }
    }
    if (depth != 0)
        return result;

    const QStringView inner = t.sliced(idx + 1, end - idx - 1);
    QStringView rest = t.sliced(end + 1).trimmed();
    if (rest.startsWith(QLatin1Char(',')))
        rest = rest.sliced(1).trimmed();

    std::vector<double> args;
    if (!parse_numbers_csv(inner, args))
        return result;

    QTransform chunk;
    if (matched_name == QLatin1String("translate")) {
        const double dx = args.size() > 0 ? args[0] : 0;
        const double dy = args.size() > 1 ? args[1] : 0;
        chunk.translate(dx, dy);
    } else if (matched_name == QLatin1String("scale")) {
        const double sx = args.size() > 0 ? args[0] : 1;
        const double sy = args.size() > 1 ? args[1] : sx;
        chunk.scale(sx, sy);
    } else if (matched_name == QLatin1String("rotate")) {
        const double ang = args.size() > 0 ? args[0] : 0;
        if (args.size() >= 3) {
            chunk.translate(args[1], args[2]);
            chunk.rotate(ang);
            chunk.translate(-args[1], -args[2]);
        } else {
            chunk.rotate(ang);
        }
    } else if (matched_name == QLatin1String("skewX")) {
        const double a = args.size() > 0 ? args[0] : 0;
        chunk.shear(std::tan(a * M_PI / 180.0), 0);
    } else if (matched_name == QLatin1String("skewY")) {
        const double a = args.size() > 0 ? args[0] : 0;
        chunk.shear(0, std::tan(a * M_PI / 180.0));
    } else if (matched_name == QLatin1String("matrix")) {
        if (args.size() >= 6)
            chunk = QTransform(args[0], args[1], args[2], args[3], args[4], args[5]);
    }

    const QTransform tail = parseTransformChain(rest);
    return tail * chunk;
}

bool appendPointsPath(QStringView points_attr, QPainterPath& out, bool close_poly)
{
    QString flat =
        QString::fromUtf16(points_attr.utf16(), points_attr.size()).replace(QLatin1Char(','), QLatin1Char(' '));
    const QStringList tok = flat.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (tok.size() < 4 || (tok.size() % 2) != 0)
        return false;

    bool ok = false;
    const double x0 = QLocale::c().toDouble(tok[0], &ok);
    if (!ok)
        return false;
    const double y0 = QLocale::c().toDouble(tok[1], &ok);
    if (!ok)
        return false;
    out.moveTo(x0, y0);
    for (int i = 2; i + 1 < tok.size(); i += 2) {
        const double x = QLocale::c().toDouble(tok[i], &ok);
        if (!ok)
            return false;
        const double y = QLocale::c().toDouble(tok[i + 1], &ok);
        if (!ok)
            return false;
        out.lineTo(x, y);
    }
    if (close_poly)
        out.closeSubpath();
    return true;
}

void collectIds(const QDomElement& el, QHash<QString, QDomElement>& map)
{
    const QString id = el.attribute(QStringLiteral("id"));
    if (!id.isEmpty())
        map.insert(id, el);
    for (QDomNode n = el.firstChild(); !n.isNull(); n = n.nextSibling()) {
        if (n.isElement())
            collectIds(n.toElement(), map);
    }
}

struct WalkCtx {
    QHash<QString, QDomElement>* ids = nullptr;
    int use_depth = 0;
    static constexpr int kMaxUseDepth = 96;
    QString document_base_dir;
    QStringList* warnings = nullptr;
    bool warned_fill_url = false;
    bool warned_stroke_url = false;
    bool warned_filter = false;
    bool notified_mask_approx = false;
};

static void appendSvgWarning(WalkCtx& ctx, const QString& msg)
{
    if (!ctx.warnings || ctx.warnings->size() >= 48)
        return;
    ctx.warnings->append(msg);
}

static QString resolveHrefFilesystem(const QString& href_raw, const WalkCtx& ctx)
{
    QString href = href_raw.trimmed();
    if (href.startsWith(QLatin1String("data:"), Qt::CaseInsensitive))
        return {};
    if (href.startsWith(QLatin1String("http:"), Qt::CaseInsensitive)
        || href.startsWith(QLatin1String("https:"), Qt::CaseInsensitive))
        return {};
    if (href.startsWith(QLatin1String("file:"), Qt::CaseInsensitive))
        href = QUrl(href).toLocalFile();

    QFileInfo fi(href);
    if (fi.isAbsolute())
        return fi.absoluteFilePath();
    if (!ctx.document_base_dir.isEmpty())
        return QFileInfo(QDir(ctx.document_base_dir).absoluteFilePath(href)).absoluteFilePath();
    return QFileInfo(href).absoluteFilePath();
}

static QString hrefFragment(const QString& href_raw);

static bool paintUrlReferencesDef(const QString& paint, WalkCtx& ctx, QString* def_tag_out)
{
    if (!paint.startsWith(QLatin1String("url("), Qt::CaseInsensitive))
        return false;
    if (!ctx.ids)
        return false;
    const QString id = hrefFragment(paint);
    if (id.isEmpty() || !ctx.ids->contains(id))
        return false;
    const QString tag = ctx.ids->value(id).tagName().toLower();
    if (def_tag_out)
        *def_tag_out = tag;
    return tag == QLatin1String("lineargradient") || tag == QLatin1String("radialgradient")
           || tag == QLatin1String("pattern");
}

static QImage loadImageHref(const QString& href_raw, WalkCtx& ctx)
{
    const QString href = href_raw.trimmed();
    if (href.startsWith(QLatin1String("data:"), Qt::CaseInsensitive)) {
        const int comma = href.indexOf(QLatin1Char(','));
        if (comma < 0)
            return {};
        QByteArray payload = href.mid(comma + 1).toUtf8();
        if (href.mid(comma - 7, 7).contains(QLatin1String("base64"), Qt::CaseInsensitive))
            payload = QByteArray::fromBase64(payload);
        QImage img;
        if (img.loadFromData(payload))
            return img;
        return {};
    }
    const QString fs = resolveHrefFilesystem(href, ctx);
    if (fs.isEmpty())
        return {};
    QImage img(fs);
    return img;
}

static QString hrefFragment(const QString& href_raw)
{
    QString href = href_raw.trimmed();
    if (href.startsWith(QLatin1String("url("), Qt::CaseInsensitive)) {
        const int hash = href.indexOf(QLatin1Char('#'));
        if (hash >= 0) {
            href = href.mid(hash + 1);
            if (href.endsWith(QLatin1Char(')')))
                href.chop(1);
            href = href.trimmed();
        }
    }
    if (href.startsWith(QLatin1Char('#')))
        href = href.mid(1);
    return href;
}

QTransform svgViewportTransform(const QDomElement& svg_el);
QTransform symbolInstanceTransform(const QDomElement& symbol, const QDomElement& use_el);

QString extractClipPathId(const QDomElement& el)
{
    QString raw = el.attribute(QStringLiteral("clip-path")).trimmed();
    if (raw.isEmpty()) {
        const QString style = el.attribute(QStringLiteral("style"));
        const QStringList decls = style.split(QLatin1Char(';'), Qt::SkipEmptyParts);
        for (const QString& d : decls) {
            const int colon = d.indexOf(QLatin1Char(':'));
            if (colon <= 0)
                continue;
            const QString key = d.left(colon).trimmed().toLower();
            if (key == QLatin1String("clip-path"))
                raw = d.mid(colon + 1).trimmed();
        }
    }
    if (raw.isEmpty() || raw.compare(QLatin1String("none"), Qt::CaseInsensitive) == 0)
        return {};
    return hrefFragment(raw);
}

QString extractStyleProperty(const QString& style, const QString& keyLower)
{
    const QStringList decls = style.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString& d : decls) {
        const int colon = d.indexOf(QLatin1Char(':'));
        if (colon <= 0)
            continue;
        if (d.left(colon).trimmed().toLower() != keyLower)
            continue;
        return d.mid(colon + 1).trimmed();
    }
    return {};
}

QString presentationAttribute(const QDomElement& el, const QString& propName)
{
    QString v = el.attribute(propName).trimmed();
    if (!v.isEmpty())
        return v;
    return extractStyleProperty(el.attribute(QStringLiteral("style")), propName.toLower());
}

QString extractMaskId(const QDomElement& el)
{
    QString raw = presentationAttribute(el, QStringLiteral("mask"));
    if (raw.isEmpty() || raw.compare(QLatin1String("none"), Qt::CaseInsensitive) == 0)
        return {};
    return hrefFragment(raw);
}

enum class AccumKind { ClipDefinition, BoundsApprox };

enum class PrimitivePieceResult { Ok, Skip, Fail };

PrimitivePieceResult appendPrimitivePiece(const QDomElement& el, QPainterPath& piece, QString* err,
                                          WalkCtx& ctx)
{
    auto failPath = [&](const QString& msg) {
        if (err)
            *err = msg;
        return PrimitivePieceResult::Fail;
    };

    const QString tag = el.tagName().toLower();

    if (tag == QLatin1String("path")) {
        const QString d = el.attribute(QStringLiteral("d"));
        if (!append_svg_path_d(piece, QStringView(d)))
            return failPath(QStringLiteral("Invalid SVG <path> data"));
        return PrimitivePieceResult::Ok;
    }
    if (tag == QLatin1String("line")) {
        const double x1 = parseUnit(QStringView(el.attribute(QStringLiteral("x1"))));
        const double y1 = parseUnit(QStringView(el.attribute(QStringLiteral("y1"))));
        const double x2 = parseUnit(QStringView(el.attribute(QStringLiteral("x2"))));
        const double y2 = parseUnit(QStringView(el.attribute(QStringLiteral("y2"))));
        piece.moveTo(x1, y1);
        piece.lineTo(x2, y2);
        return PrimitivePieceResult::Ok;
    }
    if (tag == QLatin1String("polyline")) {
        if (!appendPointsPath(QStringView(el.attribute(QStringLiteral("points"))), piece, false))
            return failPath(QStringLiteral("Invalid <polyline> points"));
        return PrimitivePieceResult::Ok;
    }
    if (tag == QLatin1String("polygon")) {
        if (!appendPointsPath(QStringView(el.attribute(QStringLiteral("points"))), piece, true))
            return failPath(QStringLiteral("Invalid <polygon> points"));
        return PrimitivePieceResult::Ok;
    }
    if (tag == QLatin1String("rect")) {
        const double x = parseUnit(QStringView(el.attribute(QStringLiteral("x"))));
        const double y = parseUnit(QStringView(el.attribute(QStringLiteral("y"))));
        const double w = parseUnit(QStringView(el.attribute(QStringLiteral("width"))));
        const double h = parseUnit(QStringView(el.attribute(QStringLiteral("height"))));
        if (w <= 0 || h <= 0)
            return PrimitivePieceResult::Skip;
        piece.addRect(x, y, w, h);
        return PrimitivePieceResult::Ok;
    }
    if (tag == QLatin1String("circle")) {
        const double cx = parseUnit(QStringView(el.attribute(QStringLiteral("cx"))));
        const double cy = parseUnit(QStringView(el.attribute(QStringLiteral("cy"))));
        const double r = parseUnit(QStringView(el.attribute(QStringLiteral("r"))));
        if (r <= 0)
            return PrimitivePieceResult::Skip;
        piece.addEllipse(QPointF(cx, cy), r, r);
        return PrimitivePieceResult::Ok;
    }
    if (tag == QLatin1String("ellipse")) {
        const double cx = parseUnit(QStringView(el.attribute(QStringLiteral("cx"))));
        const double cy = parseUnit(QStringView(el.attribute(QStringLiteral("cy"))));
        const double rx = parseUnit(QStringView(el.attribute(QStringLiteral("rx"))));
        const double ry = parseUnit(QStringView(el.attribute(QStringLiteral("ry"))));
        if (rx <= 0 || ry <= 0)
            return PrimitivePieceResult::Skip;
        piece.addEllipse(QPointF(cx, cy), rx, ry);
        return PrimitivePieceResult::Ok;
    }
    if (tag == QLatin1String("image")) {
        QString href = el.attribute(QStringLiteral("href"));
        if (href.isEmpty())
            href = el.attribute(QStringLiteral("xlink:href"));
        const double x = parseUnit(QStringView(el.attribute(QStringLiteral("x"))));
        const double y = parseUnit(QStringView(el.attribute(QStringLiteral("y"))));
        const double w = parseUnit(QStringView(el.attribute(QStringLiteral("width"))));
        const double h = parseUnit(QStringView(el.attribute(QStringLiteral("height"))));
        if (w <= 0 || h <= 0) {
            appendSvgWarning(ctx, QStringLiteral("<image>: brak lub zerowy width/height — pominięto."));
            return PrimitivePieceResult::Skip;
        }
        const QString tr = href.trimmed();
        if (tr.startsWith(QLatin1String("http:"), Qt::CaseInsensitive)
            || tr.startsWith(QLatin1String("https:"), Qt::CaseInsensitive)) {
            appendSvgWarning(ctx,
                             QStringLiteral("<image>: URI sieciowe nie są pobierane (prostokąt obramowania)."));
            piece.addRect(x, y, w, h);
            return PrimitivePieceResult::Ok;
        }

        QImage img = loadImageHref(href, ctx);
        if (!img.isNull() && isBitmapTracingAvailable()) {
            QPainterPath traced;
            QString terr;
            if (traceBitmapToPath(img, traced, &terr) && !traced.isEmpty()) {
                const QRectF tb = traced.boundingRect();
                if (tb.width() > 1e-6 && tb.height() > 1e-6) {
                    QTransform map;
                    map.translate(x, y);
                    map.scale(w / tb.width(), h / tb.height());
                    map.translate(-tb.left(), -tb.top());
                    piece.addPath(map.map(traced));
                    return PrimitivePieceResult::Ok;
                }
            }
            if (!terr.isEmpty())
                appendSvgWarning(ctx, QStringLiteral("<image> potrace: %1").arg(terr));
        } else if (!img.isNull() && !isBitmapTracingAvailable()) {
            appendSvgWarning(ctx,
                             QStringLiteral("<image>: brak libpotrace — użyto prostokąta obramowania."));
        } else if (!tr.startsWith(QLatin1String("data:"), Qt::CaseInsensitive)) {
            const QString fs = resolveHrefFilesystem(href, ctx);
            if (!fs.isEmpty() && !QFile::exists(fs))
                appendSvgWarning(ctx, QStringLiteral("<image>: nie znaleziono pliku „%1”.").arg(href));
        }

        piece.addRect(x, y, w, h);
        return PrimitivePieceResult::Ok;
    }
    if (tag == QLatin1String("foreignobject")) {
        appendSvgWarning(ctx,
                         QStringLiteral("<foreignObject>: zawartość niestandardowa pominięta (opcjonalne obramowanie)."));
        const double x = parseUnit(QStringView(el.attribute(QStringLiteral("x"))));
        const double y = parseUnit(QStringView(el.attribute(QStringLiteral("y"))));
        const double w = parseUnit(QStringView(el.attribute(QStringLiteral("width"))));
        const double h = parseUnit(QStringView(el.attribute(QStringLiteral("height"))));
        if (w > 0 && h > 0) {
            piece.addRect(x, y, w, h);
            return PrimitivePieceResult::Ok;
        }
        return PrimitivePieceResult::Skip;
    }
    if (tag == QLatin1String("text")) {
        return PrimitivePieceResult::Skip;
    }
    return PrimitivePieceResult::Skip;
}

static QFont fontMergePresentation(const QDomElement& el, const QFont& inherit)
{
    QFont f = inherit;

    QString ff = presentationAttribute(el, QStringLiteral("font-family"));
    ff = ff.split(QLatin1Char(',')).value(0).trimmed();
    ff.remove(QLatin1Char('"'));
    ff.remove(QLatin1Char('\''));
    if (!ff.isEmpty())
        f.setFamily(ff);

    const double fs =
        parseUnit(QStringView(presentationAttribute(el, QStringLiteral("font-size"))), -1.0);
    if (fs > 0)
        f.setPixelSize(qMax(1, static_cast<int>(std::round(fs))));
    return f;
}

static QFont rootTextFont(const QDomElement& text_el)
{
    QFont base;
    base.setFamily(QStringLiteral("sans-serif"));
    base.setPixelSize(16);
    return fontMergePresentation(text_el, base);
}

static QVector<double> parseLengthListTokens(QStringView s)
{
    QVector<double> out;
    QString token;
    auto flush = [&]() {
        const QStringView v = QStringView(token).trimmed();
        token.clear();
        if (!v.isEmpty())
            out.push_back(parseUnit(v));
    };
    for (int i = 0; i <= s.size(); ++i) {
        const QChar c = i < s.size() ? s.at(i) : QChar();
        if (i == s.size() || c.isSpace() || c == QLatin1Char(',')) {
            flush();
            continue;
        }
        token.append(c);
    }
    return out;
}

static double coordListTile(const QVector<double>& list, int idx)
{
    if (list.isEmpty())
        return 0;
    const int j = qBound(0, idx, list.size() - 1);
    return list[j];
}

static void mergeCoordListsFromElement(const QDomElement& el, QVector<double>& x, QVector<double>& y,
                                       QVector<double>& dx, QVector<double>& dy)
{
    const QString xa = el.attribute(QStringLiteral("x")).trimmed();
    if (!xa.isEmpty())
        x = parseLengthListTokens(QStringView(xa));
    const QString ya = el.attribute(QStringLiteral("y")).trimmed();
    if (!ya.isEmpty())
        y = parseLengthListTokens(QStringView(ya));
    const QString dxa = el.attribute(QStringLiteral("dx")).trimmed();
    if (!dxa.isEmpty())
        dx = parseLengthListTokens(QStringView(dxa));
    const QString dya = el.attribute(QStringLiteral("dy")).trimmed();
    if (!dya.isEmpty())
        dy = parseLengthListTokens(QStringView(dya));
}

static double parseTextPathStartOffsetPx(QStringView s, double path_len)
{
    s = s.trimmed();
    if (s.isEmpty() || path_len <= 0)
        return 0;
    if (s.endsWith(QLatin1Char('%'))) {
        bool ok = false;
        const double v = QLocale::c().toDouble(QStringView(s).left(s.size() - 1).trimmed(), &ok);
        if (!ok)
            return 0;
        return qBound(0.0, v / 100.0 * path_len, path_len);
    }
    return qBound(0.0, parseUnit(s), path_len);
}

static double appendTextAlongPathOutlineDist(QStringView text, const QPainterPath& path, double distAlong,
                                             const QFont& font, bool flipSide, QPainterPath& accum)
{
    if (text.isEmpty() || path.isEmpty())
        return distAlong;
    const qreal plen = path.length();
    if (plen < 1e-9)
        return distAlong;

    QFontMetricsF fm(font);

    for (int i = 0; i < text.size(); ++i) {
        const QChar ch = text[i];
        const QString one = QString(ch);
        const double adv = fm.horizontalAdvance(one);

        if (ch.isSpace()) {
            distAlong += adv;
            if (distAlong > plen)
                break;
            continue;
        }

        if (adv <= 1e-9) {
            distAlong += adv;
            continue;
        }

        distAlong = qBound(0.0, distAlong, double(plen));
        const qreal t_param = path.percentAtLength(qreal(distAlong));
        const QPointF pt = path.pointAtPercent(t_param);
        const qreal tang = path.angleAtPercent(t_param);

        QPainterPath glyph;
        glyph.addText(QPointF(0, 0), font, one);

        QTransform xf;
        xf.translate(pt.x(), pt.y());
        xf.rotate(tang + (flipSide ? 180.0 : 0.0));
        accum.addPath(xf.map(glyph));

        distAlong += adv;
        if (distAlong > plen)
            break;
    }

    return distAlong;
}

static double walkTextPathContent(QDomNode n, bool preserve_space, const QPainterPath& pp, double distAlong,
                                  const QFont& font, bool flipSide, QPainterPath& accum)
{
    if (n.isText()) {
        QString t = n.toText().data();
        if (!preserve_space) {
            t.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
            t = t.trimmed();
        }
        return appendTextAlongPathOutlineDist(QStringView(t), pp, distAlong, font, flipSide, accum);
    }

    if (!n.isElement())
        return distAlong;

    const QDomElement el = n.toElement();
    const QString tag = el.tagName().toLower();

    if (tag == QLatin1String("tspan")) {
        bool ps = preserve_space;
        const QString xs = el.attribute(QStringLiteral("xml:space")).trimmed().toLower();
        if (xs == QLatin1String("preserve"))
            ps = true;

        const QFont loc = fontMergePresentation(el, font);

        for (QDomNode c = el.firstChild(); !c.isNull(); c = c.nextSibling())
            distAlong = walkTextPathContent(c, ps, pp, distAlong, loc, flipSide, accum);
        return distAlong;
    }

    return distAlong;
}

struct TextOutlineState {
    double penX = 0;
    double penY = 0;
    int glyph_slot = 0;
    QVector<double> x;
    QVector<double> y;
    QVector<double> dx;
    QVector<double> dy;
};

static void applyGlyphCoords(TextOutlineState& st)
{
    if (!st.x.isEmpty())
        st.penX = coordListTile(st.x, st.glyph_slot);
    if (!st.y.isEmpty())
        st.penY = coordListTile(st.y, st.glyph_slot);
    if (!st.dx.isEmpty())
        st.penX += coordListTile(st.dx, st.glyph_slot);
    if (!st.dy.isEmpty())
        st.penY += coordListTile(st.dy, st.glyph_slot);
}

static void walkTextOutlineNodes(QDomNode n, TextOutlineState& st, const QFont& font,
                                 bool preserve_space, QPainterPath& accum, WalkCtx& ctx)
{
    const QFontMetricsF fm(font);

    if (n.isText()) {
        QString t = n.toText().data();
        if (!preserve_space) {
            t.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
            t = t.trimmed();
        }
        for (QChar ch : t) {
            if (!preserve_space && ch.isSpace()) {
                st.penX += fm.horizontalAdvance(QString(ch));
                continue;
            }
            applyGlyphCoords(st);
            QPainterPath frag;
            frag.addText(QPointF(st.penX, st.penY), font, QString(ch));
            accum.addPath(frag);
            st.penX += fm.horizontalAdvance(QString(ch));
            ++st.glyph_slot;
        }
        return;
    }

    if (!n.isElement())
        return;

    const QDomElement el = n.toElement();
    const QString tag = el.tagName().toLower();

    if (tag == QLatin1String("textpath")) {
        QString href = el.attribute(QStringLiteral("href"));
        if (href.isEmpty())
            href = el.attribute(QStringLiteral("xlink:href"));
        const QString id = hrefFragment(href);
        const QDomElement path_el = ctx.ids ? ctx.ids->value(id) : QDomElement();
        if (path_el.isNull()
            || path_el.tagName().compare(QLatin1String("path"), Qt::CaseInsensitive) != 0)
            return;

        QPainterPath pp;
        const QString d = path_el.attribute(QStringLiteral("d"));
        if (!append_svg_path_d(pp, QStringView(d)))
            return;

        const double off =
            parseTextPathStartOffsetPx(QStringView(el.attribute(QStringLiteral("startOffset"))),
                                       pp.length());

        const bool flipSide =
            el.attribute(QStringLiteral("side")).trimmed().compare(QLatin1String("right"),
                                                                   Qt::CaseInsensitive)
            == 0;

        bool ps = preserve_space;
        const QString xs = el.attribute(QStringLiteral("xml:space")).trimmed().toLower();
        if (xs == QLatin1String("preserve"))
            ps = true;

        const QFont flocal = fontMergePresentation(el, font);

        double cursor = off;
        for (QDomNode c = el.firstChild(); !c.isNull(); c = c.nextSibling())
            cursor = walkTextPathContent(c, ps, pp, cursor, flocal, flipSide, accum);

        return;
    }

    if (tag == QLatin1String("tspan")) {
        const QVector<double> sx = st.x;
        const QVector<double> sy = st.y;
        const QVector<double> sdx = st.dx;
        const QVector<double> sdy = st.dy;

        mergeCoordListsFromElement(el, st.x, st.y, st.dx, st.dy);

        bool ps = preserve_space;
        const QString xs = el.attribute(QStringLiteral("xml:space")).trimmed().toLower();
        if (xs == QLatin1String("preserve"))
            ps = true;

        const QFont flocal = fontMergePresentation(el, font);

        for (QDomNode c = el.firstChild(); !c.isNull(); c = c.nextSibling())
            walkTextOutlineNodes(c, st, flocal, ps, accum, ctx);

        st.x = sx;
        st.y = sy;
        st.dx = sdx;
        st.dy = sdy;
        return;
    }

    /* Inne elementy pod <text> pomijamy (np. <title>). */
}

bool appendTextOutline(const QDomElement& el, QPainterPath& piece, QString* /*err*/, WalkCtx& ctx)
{
    TextOutlineState st;
    mergeCoordListsFromElement(el, st.x, st.y, st.dx, st.dy);

    const bool preserve_space =
        el.attribute(QStringLiteral("xml:space")).trimmed().compare(QLatin1String("preserve"),
                                                                     Qt::CaseInsensitive)
        == 0;

    st.penX =
        st.x.isEmpty() ? parseUnit(QStringView(el.attribute(QStringLiteral("x")))) : st.x.front();
    st.penY =
        st.y.isEmpty() ? parseUnit(QStringView(el.attribute(QStringLiteral("y")))) : st.y.front();

    const QFont rootFont = rootTextFont(el);

    QPainterPath accum;
    for (QDomNode n = el.firstChild(); !n.isNull(); n = n.nextSibling())
        walkTextOutlineNodes(n, st, rootFont, preserve_space, accum, ctx);

    if (accum.isEmpty())
        return false;

    QString anchor = presentationAttribute(el, QStringLiteral("text-anchor")).trimmed().toLower();
    if (anchor == QLatin1String("middle") || anchor == QLatin1String("center")) {
        const QRectF b = accum.boundingRect();
        accum.translate(-b.width() / 2.0, 0);
    } else if (anchor == QLatin1String("end")) {
        const QRectF b = accum.boundingRect();
        accum.translate(-b.width(), 0);
    }

    piece.addPath(accum);
    return true;
}

void uniteMappedPiece(QPainterPath& acc, const QPainterPath& piece, const QTransform& xf)
{
    if (piece.isEmpty())
        return;
    const QPainterPath mapped = xf.map(piece);
    acc = acc.isEmpty() ? mapped : acc.united(mapped);
}

void applyPresentationPaint(const QDomElement& el, QPainterPath& piece, WalkCtx& ctx)
{
    if (piece.isEmpty())
        return;

    QString filter_attr = presentationAttribute(el, QStringLiteral("filter")).trimmed();
    if (!filter_attr.isEmpty() && filter_attr.compare(QLatin1String("none"), Qt::CaseInsensitive) != 0
        && !ctx.warned_filter) {
        QString ftag;
        if (paintUrlReferencesDef(filter_attr, ctx, &ftag)) {
            appendSvgWarning(ctx,
                             QStringLiteral("Filtry SVG (filter=%1) — geometria bez rozmycia/efektów.")
                                 .arg(ftag));
        } else {
            appendSvgWarning(ctx,
                             QStringLiteral("Filtry SVG są ignorowane — geometria bez efektów."));
        }
        ctx.warned_filter = true;
    }

    QString fill = presentationAttribute(el, QStringLiteral("fill")).trimmed();
    if (fill.isEmpty())
        fill = QStringLiteral("#000000");

    QString stroke = presentationAttribute(el, QStringLiteral("stroke")).trimmed();

    auto is_none = [](const QString& s) {
        return s.compare(QLatin1String("none"), Qt::CaseInsensitive) == 0;
    };
    auto is_url = [](const QString& s) {
        return s.startsWith(QLatin1String("url("), Qt::CaseInsensitive);
    };

    bool fillOn = !is_none(fill)
        && !fill.startsWith(QLatin1String("transparent"), Qt::CaseInsensitive);
    if (is_url(fill)) {
        QString def_tag;
        if (paintUrlReferencesDef(fill, ctx, &def_tag)) {
            fillOn = true;
        } else if (!ctx.warned_fill_url) {
            fillOn = true;
            appendSvgWarning(ctx,
                             QStringLiteral("Wypełnienie url(#…) — nieznany odniesienie; użyto obrysu."));
            ctx.warned_fill_url = true;
        }
    }

    bool strokeOn = !stroke.isEmpty() && !is_none(stroke);
    if (is_url(stroke)) {
        QString def_tag;
        if (paintUrlReferencesDef(stroke, ctx, &def_tag)) {
            strokeOn = true;
        } else if (!ctx.warned_stroke_url) {
            strokeOn = true;
            appendSvgWarning(ctx,
                             QStringLiteral("Obrys url(#…) — nieznane odniesienie; użyto obrysu."));
            ctx.warned_stroke_url = true;
        }
    }

    const double sw =
        parseUnit(QStringView(presentationAttribute(el, QStringLiteral("stroke-width"))), 1.0);

    QPainterPath original = piece;
    QPainterPath out;
    if (fillOn)
        out.addPath(original);
    if (strokeOn && sw > 0) {
        QPainterPathStroker stroker;
        stroker.setWidth(sw);
        stroker.setCapStyle(Qt::FlatCap);
        stroker.setJoinStyle(Qt::MiterJoin);
        stroker.setMiterLimit(4);
        out.addPath(stroker.createStroke(original));
    }

    if (!fillOn && !(strokeOn && sw > 0))
        piece = QPainterPath();
    else
        piece = out;
}

void accumulateSubtree(const QDomElement& el, const QTransform& xf, QPainterPath& acc,
                       QString* err, WalkCtx& ctx, AccumKind kind);

void accumulateUseSubtree(const QDomElement& use_el, const QTransform& xf, QPainterPath& acc,
                          QString* err, WalkCtx& ctx, AccumKind kind)
{
    if (++ctx.use_depth > WalkCtx::kMaxUseDepth) {
        --ctx.use_depth;
        return;
    }

    QString href = use_el.attribute(QStringLiteral("href"));
    if (href.isEmpty())
        href = use_el.attribute(QStringLiteral("xlink:href"));

    const QString id = hrefFragment(href);
    if (id.isEmpty()) {
        if (err)
            *err = QStringLiteral("Brak href w <use>");
        --ctx.use_depth;
        return;
    }

    const QDomElement ref = ctx.ids->value(id);
    if (ref.isNull()) {
        if (err)
            *err = QStringLiteral("Nie znaleziono id=\"%1\" dla <use>").arg(id);
        --ctx.use_depth;
        return;
    }

    QTransform local =
        xf * parseTransformChain(QStringView(use_el.attribute(QStringLiteral("transform"))));

    const double ux = parseUnit(QStringView(use_el.attribute(QStringLiteral("x"))));
    const double uy = parseUnit(QStringView(use_el.attribute(QStringLiteral("y"))));
    local *= QTransform().translate(ux, uy);

    const QString rtag = ref.tagName().toLower();

    if (rtag == QLatin1String("symbol")) {
        const QTransform sym_t = symbolInstanceTransform(ref, use_el);
        const QTransform base_sym = local * sym_t;
        for (QDomNode n = ref.firstChild(); !n.isNull(); n = n.nextSibling()) {
            if (!n.isElement())
                continue;
            accumulateSubtree(n.toElement(), base_sym, acc, err, ctx, kind);
            if (err && !err->isEmpty())
                break;
        }
    } else if (rtag == QLatin1String("svg")) {
        QTransform inner = local * svgViewportTransform(ref);
        inner *= parseTransformChain(QStringView(ref.attribute(QStringLiteral("transform"))));
        for (QDomNode n = ref.firstChild(); !n.isNull(); n = n.nextSibling()) {
            if (!n.isElement())
                continue;
            accumulateSubtree(n.toElement(), inner, acc, err, ctx, kind);
            if (err && !err->isEmpty())
                break;
        }
    } else if (rtag == QLatin1String("g")) {
        accumulateSubtree(ref, local, acc, err, ctx, kind);
    } else {
        QPainterPath piece;
        const PrimitivePieceResult pr = appendPrimitivePiece(ref, piece, err, ctx);
        if (pr == PrimitivePieceResult::Fail) {
            --ctx.use_depth;
            return;
        }
        if (kind == AccumKind::BoundsApprox
            && ref.tagName().compare(QLatin1String("text"), Qt::CaseInsensitive) == 0) {
            QPainterPath tp;
            if (appendTextOutline(ref, tp, err, ctx) && !tp.isEmpty()) {
                const QTransform el_local =
                    local * parseTransformChain(QStringView(ref.attribute(QStringLiteral("transform"))));
                uniteMappedPiece(acc, tp, el_local);
            }
        } else if (pr == PrimitivePieceResult::Ok) {
            const QTransform el_local =
                local * parseTransformChain(QStringView(ref.attribute(QStringLiteral("transform"))));
            uniteMappedPiece(acc, piece, el_local);
        }
    }

    --ctx.use_depth;
}

void accumulateSubtree(const QDomElement& el, const QTransform& xf, QPainterPath& acc,
                       QString* err, WalkCtx& ctx, AccumKind kind)
{
    const QString tag = el.tagName().toLower();

    if (tag == QLatin1String("defs") || tag == QLatin1String("title") || tag == QLatin1String("desc")
        || tag == QLatin1String("metadata") || tag == QLatin1String("style")
        || tag == QLatin1String("script") || tag == QLatin1String("pattern")
        || tag == QLatin1String("marker") || tag == QLatin1String("clipPath")
        || tag == QLatin1String("mask") || tag == QLatin1String("linearGradient")
        || tag == QLatin1String("radialGradient"))
        return;

    if (tag == QLatin1String("symbol"))
        return;

    if (tag == QLatin1String("use")) {
        accumulateUseSubtree(el, xf, acc, err, ctx, kind);
        return;
    }

    if (tag == QLatin1String("svg")) {
        QTransform base = xf * svgViewportTransform(el);
        base *= parseTransformChain(QStringView(el.attribute(QStringLiteral("transform"))));
        for (QDomNode n = el.firstChild(); !n.isNull(); n = n.nextSibling()) {
            if (!n.isElement())
                continue;
            accumulateSubtree(n.toElement(), base, acc, err, ctx, kind);
            if (err && !err->isEmpty())
                return;
        }
        return;
    }

    const QTransform local =
        xf * parseTransformChain(QStringView(el.attribute(QStringLiteral("transform"))));

    if (tag == QLatin1String("g")) {
        for (QDomNode n = el.firstChild(); !n.isNull(); n = n.nextSibling()) {
            if (!n.isElement())
                continue;
            accumulateSubtree(n.toElement(), local, acc, err, ctx, kind);
            if (err && !err->isEmpty())
                return;
        }
        return;
    }

    if (kind == AccumKind::BoundsApprox
        && tag.compare(QLatin1String("text"), Qt::CaseInsensitive) == 0) {
        QPainterPath tp;
        if (appendTextOutline(el, tp, err, ctx) && !tp.isEmpty())
            uniteMappedPiece(acc, tp, local);
        return;
    }

    QPainterPath piece;
    const PrimitivePieceResult pr = appendPrimitivePiece(el, piece, err, ctx);
    if (pr == PrimitivePieceResult::Fail)
        return;
    if (pr == PrimitivePieceResult::Ok)
        uniteMappedPiece(acc, piece, local);
}

void accumulateClipDefinition(const QDomElement& clipPathEl, QPainterPath& acc, QString* err,
                              WalkCtx& ctx)
{
    const QTransform clipRoot =
        parseTransformChain(QStringView(clipPathEl.attribute(QStringLiteral("transform"))));
    for (QDomNode n = clipPathEl.firstChild(); !n.isNull(); n = n.nextSibling()) {
        if (!n.isElement())
            continue;
        accumulateSubtree(n.toElement(), clipRoot, acc, err, ctx, AccumKind::ClipDefinition);
        if (err && !err->isEmpty())
            return;
    }
}

QRectF worldSubtreeBounds(const QDomElement& el, const QTransform& localToWorld, WalkCtx& ctx,
                          QString* err)
{
    const QTransform tEl = parseTransformChain(QStringView(el.attribute(QStringLiteral("transform"))));
    const QTransform xfParent = localToWorld * tEl.inverted();
    QPainterPath acc;
    accumulateSubtree(el, xfParent, acc, err, ctx, AccumKind::BoundsApprox);
    return acc.boundingRect();
}

bool buildClipOrMaskWorld(const QDomElement& referencingEl, const QDomElement& defEl,
                          const QTransform& localToWorld, const QString& unitsAttr,
                          QPainterPath& outWorld, QString* err, WalkCtx& ctx)
{
    const QString units = unitsAttr.trimmed().toLower();
    const bool obb = (units == QLatin1String("objectboundingbox"));

    QPainterPath localDef;
    accumulateClipDefinition(defEl, localDef, err, ctx);
    if (err && !err->isEmpty())
        return false;

    if (!obb) {
        outWorld = localToWorld.map(localDef);
        return true;
    }

    const QRectF wb = worldSubtreeBounds(referencingEl, localToWorld, ctx, err);
    if (err && !err->isEmpty())
        return false;
    if (!wb.isValid() || wb.width() <= 0 || wb.height() <= 0)
        return false;

    QTransform m;
    m.translate(wb.left(), wb.top());
    m.scale(wb.width(), wb.height());
    outWorld = m.map(localDef);
    return true;
}

void resolveVisibilityClip(const QDomElement& el, const QTransform& localToWorld,
                           const QPainterPath* parentClip, std::optional<QPainterPath>& mergedBuf,
                           const QPainterPath*& effectiveClip, QString* err, WalkCtx& ctx)
{
    mergedBuf.reset();
    effectiveClip = parentClip;

    bool anyLayer = false;
    QPainterPath combined;

    auto intersectLayer = [&](const QPainterPath& layer) {
        if (layer.isEmpty())
            return;
        if (!anyLayer) {
            combined = layer;
            anyLayer = true;
        } else {
            combined = combined.intersected(layer);
        }
    };

    if (parentClip && !parentClip->isEmpty())
        intersectLayer(*parentClip);

    const QString clipId = extractClipPathId(el);
    if (!clipId.isEmpty()) {
        const QDomElement clipDef = ctx.ids->value(clipId);
        if (!clipDef.isNull()
            && clipDef.tagName().compare(QLatin1String("clipPath"), Qt::CaseInsensitive) == 0) {
            QPainterPath cw;
            if (buildClipOrMaskWorld(el, clipDef, localToWorld,
                                     clipDef.attribute(QStringLiteral("clipPathUnits")), cw, err,
                                     ctx))
                intersectLayer(cw);
        }
    }

    const QString maskId = extractMaskId(el);
    if (!maskId.isEmpty()) {
        const QDomElement maskDef = ctx.ids->value(maskId);
        if (!maskDef.isNull()
            && maskDef.tagName().compare(QLatin1String("mask"), Qt::CaseInsensitive) == 0) {
            QPainterPath mw;
            if (buildClipOrMaskWorld(el, maskDef, localToWorld,
                                     maskDef.attribute(QStringLiteral("maskContentUnits")), mw, err,
                                     ctx)) {
                intersectLayer(mw);
                if (!ctx.notified_mask_approx) {
                    appendSvgWarning(ctx,
                                     QStringLiteral(
                                         "Maska SVG stosowana jako przecięcie geometryczne (luminance/alpha "
                                         "bez rasteru)."));
                    ctx.notified_mask_approx = true;
                }
            }
        }
    }

    if (!anyLayer)
        return;

    mergedBuf = std::move(combined);
    effectiveClip = &(*mergedBuf);
}

/// Maps viewBox user coordinates into viewport [0,vp_w]×[0,vp_h] (xMidYMid meet by default).
QTransform svgViewportTransform(const QDomElement& svg_el)
{
    const QString vb_str = svg_el.attribute(QStringLiteral("viewBox")).trimmed();
    double vx = 0, vy = 0, vw = 300, vh = 150;

    if (!vb_str.isEmpty()) {
        QString flat = vb_str;
        flat.replace(QLatin1Char(','), QLatin1Char(' '));
        const QStringList p = flat.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (p.size() == 4) {
            bool ok = true;
            vx = QLocale::c().toDouble(p[0], &ok);
            vy = QLocale::c().toDouble(p[1], &ok);
            vw = QLocale::c().toDouble(p[2], &ok);
            vh = QLocale::c().toDouble(p[3], &ok);
            if (!ok || vw <= 0 || vh <= 0)
                return QTransform();
        }
    } else {
        vw = parseUnit(QStringView(svg_el.attribute(QStringLiteral("width"))), 300);
        vh = parseUnit(QStringView(svg_el.attribute(QStringLiteral("height"))), 150);
        if (vw <= 0 || vh <= 0)
            return QTransform();
    }

    double vp_w = parseUnit(QStringView(svg_el.attribute(QStringLiteral("width"))), vw);
    double vp_h = parseUnit(QStringView(svg_el.attribute(QStringLiteral("height"))), vh);
    if (vp_w <= 0 || vp_h <= 0)
        return QTransform();

    const QString par = svg_el.attribute(QStringLiteral("preserveAspectRatio")).trimmed();
    if (par.startsWith(QLatin1String("none"), Qt::CaseInsensitive)) {
        QTransform t;
        t.scale(vp_w / vw, vp_h / vh);
        t.translate(-vx, -vy);
        return t;
    }

    const bool slice = par.contains(QLatin1String("slice"), Qt::CaseInsensitive);
    const double sx = vp_w / vw;
    const double sy = vp_h / vh;
    const double uni = slice ? std::max(sx, sy) : std::min(sx, sy);
    const double tx = (vp_w - vw * uni) / 2.0;
    const double ty = (vp_h - vh * uni) / 2.0;

    QTransform t;
    t.translate(tx, ty);
    t.scale(uni, uni);
    t.translate(-vx, -vy);
    return t;
}

/// Maps symbol local coordinates into use viewport when width/height given.
QTransform symbolInstanceTransform(const QDomElement& symbol, const QDomElement& use_el)
{
    const QString vb_str = symbol.attribute(QStringLiteral("viewBox")).trimmed();
    const double uw = parseUnit(QStringView(use_el.attribute(QStringLiteral("width"))), -1);
    const double uh = parseUnit(QStringView(use_el.attribute(QStringLiteral("height"))), -1);
    if (vb_str.isEmpty() || uw <= 0 || uh <= 0)
        return QTransform();

    QString flat = vb_str;
    flat.replace(QLatin1Char(','), QLatin1Char(' '));
    const QStringList p = flat.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (p.size() != 4)
        return QTransform();

    bool ok = true;
    const double mx = QLocale::c().toDouble(p[0], &ok);
    const double my = QLocale::c().toDouble(p[1], &ok);
    const double sw = QLocale::c().toDouble(p[2], &ok);
    const double sh = QLocale::c().toDouble(p[3], &ok);
    if (!ok || sw <= 0 || sh <= 0)
        return QTransform();

    const double scale_u = std::min(uw / sw, uh / sh);
    const double ox = (uw - sw * scale_u) / 2.0;
    const double oy = (uh - sh * scale_u) / 2.0;

    QTransform t;
    t.translate(ox, oy);
    t.scale(scale_u, scale_u);
    t.translate(-mx, -my);
    return t;
}

void renderSubtree(const QDomElement& el, const QTransform& xf, QPainterPath& out, QString* err,
                   WalkCtx& ctx, const QPainterPath* clipWorld);

void renderPrimitiveShape(const QDomElement& el, const QTransform& local, QPainterPath& out,
                          QString* err, WalkCtx& ctx, const QPainterPath* clipWorld);

void renderPrimitiveShape(const QDomElement& el, const QTransform& local, QPainterPath& out,
                          QString* err, WalkCtx& ctx, const QPainterPath* clipWorld)
{
    QPainterPath piece;
    const QString tag = el.tagName().toLower();

    PrimitivePieceResult pr = appendPrimitivePiece(el, piece, err, ctx);
    if (pr == PrimitivePieceResult::Fail)
        return;

    if (pr == PrimitivePieceResult::Skip) {
        if (tag.compare(QLatin1String("text"), Qt::CaseInsensitive) != 0)
            return;
        if (!appendTextOutline(el, piece, err, ctx))
            return;
    }

    applyPresentationPaint(el, piece, ctx);
    if (piece.isEmpty())
        return;

    QPainterPath worldPiece = local.map(piece);
    if (clipWorld && !clipWorld->isEmpty())
        worldPiece = worldPiece.intersected(*clipWorld);
    out.addPath(worldPiece);
}

void renderUse(const QDomElement& use_el, const QTransform& xf, QPainterPath& out, QString* err,
               WalkCtx& ctx, const QPainterPath* clipWorld)
{
    if (++ctx.use_depth > WalkCtx::kMaxUseDepth) {
        --ctx.use_depth;
        return;
    }

    QString href = use_el.attribute(QStringLiteral("href"));
    if (href.isEmpty())
        href = use_el.attribute(QStringLiteral("xlink:href"));

    const QString id = hrefFragment(href);
    if (id.isEmpty()) {
        if (err)
            *err = QStringLiteral("Brak href w <use>");
        --ctx.use_depth;
        return;
    }

    const QDomElement ref = ctx.ids->value(id);
    if (ref.isNull()) {
        if (err)
            *err = QStringLiteral("Nie znaleziono id=\"%1\" dla <use>").arg(id);
        --ctx.use_depth;
        return;
    }

    QTransform local =
        xf * parseTransformChain(QStringView(use_el.attribute(QStringLiteral("transform"))));

    const double ux = parseUnit(QStringView(use_el.attribute(QStringLiteral("x"))));
    const double uy = parseUnit(QStringView(use_el.attribute(QStringLiteral("y"))));
    local *= QTransform().translate(ux, uy);

    std::optional<QPainterPath> mergedClipBuf;
    const QPainterPath* effectiveClip = clipWorld;
    resolveVisibilityClip(use_el, local, clipWorld, mergedClipBuf, effectiveClip, err, ctx);
    if (err && !err->isEmpty()) {
        --ctx.use_depth;
        return;
    }

    const QString rtag = ref.tagName().toLower();

    if (rtag == QLatin1String("symbol")) {
        const QTransform sym_t = symbolInstanceTransform(ref, use_el);
        const QTransform base_sym = local * sym_t;
        for (QDomNode n = ref.firstChild(); !n.isNull(); n = n.nextSibling()) {
            if (!n.isElement())
                continue;
            renderSubtree(n.toElement(), base_sym, out, err, ctx, effectiveClip);
            if (err && !err->isEmpty())
                return;
        }
    } else if (rtag == QLatin1String("svg")) {
        QTransform inner = local * svgViewportTransform(ref);
        inner *= parseTransformChain(QStringView(ref.attribute(QStringLiteral("transform"))));
        for (QDomNode n = ref.firstChild(); !n.isNull(); n = n.nextSibling()) {
            if (!n.isElement())
                continue;
            renderSubtree(n.toElement(), inner, out, err, ctx, effectiveClip);
            if (err && !err->isEmpty())
                return;
        }
    } else if (rtag == QLatin1String("g")) {
        renderSubtree(ref, local, out, err, ctx, effectiveClip);
    } else {
        QTransform el_local =
            local * parseTransformChain(QStringView(ref.attribute(QStringLiteral("transform"))));
        renderPrimitiveShape(ref, el_local, out, err, ctx, effectiveClip);
    }

    --ctx.use_depth;
}

void renderSubtree(const QDomElement& el, const QTransform& xf, QPainterPath& out, QString* err,
                   WalkCtx& ctx, const QPainterPath* clipWorld)
{
    const QString tag = el.tagName().toLower();

    if (tag == QLatin1String("defs") || tag == QLatin1String("title") || tag == QLatin1String("desc")
        || tag == QLatin1String("metadata") || tag == QLatin1String("style")
        || tag == QLatin1String("script") || tag == QLatin1String("pattern")
        || tag == QLatin1String("marker") || tag == QLatin1String("clipPath")
        || tag == QLatin1String("mask") || tag == QLatin1String("linearGradient")
        || tag == QLatin1String("radialGradient"))
        return;

    if (tag == QLatin1String("symbol"))
        return;

    if (tag == QLatin1String("use")) {
        renderUse(el, xf, out, err, ctx, clipWorld);
        return;
    }

    if (tag == QLatin1String("svg")) {
        QTransform base = xf * svgViewportTransform(el);
        base *= parseTransformChain(QStringView(el.attribute(QStringLiteral("transform"))));
        std::optional<QPainterPath> mergedClipBuf;
        const QPainterPath* effectiveClip = clipWorld;
        resolveVisibilityClip(el, base, clipWorld, mergedClipBuf, effectiveClip, err, ctx);
        if (err && !err->isEmpty())
            return;
        for (QDomNode n = el.firstChild(); !n.isNull(); n = n.nextSibling()) {
            if (!n.isElement())
                continue;
            renderSubtree(n.toElement(), base, out, err, ctx, effectiveClip);
            if (err && !err->isEmpty())
                return;
        }
        return;
    }

    const QTransform local =
        xf * parseTransformChain(QStringView(el.attribute(QStringLiteral("transform"))));

    std::optional<QPainterPath> mergedClipBuf;
    const QPainterPath* effectiveClip = clipWorld;
    resolveVisibilityClip(el, local, clipWorld, mergedClipBuf, effectiveClip, err, ctx);
    if (err && !err->isEmpty())
        return;

    if (tag == QLatin1String("g")) {
        for (QDomNode n = el.firstChild(); !n.isNull(); n = n.nextSibling()) {
            if (!n.isElement())
                continue;
            renderSubtree(n.toElement(), local, out, err, ctx, effectiveClip);
            if (err && !err->isEmpty())
                return;
        }
        return;
    }

    renderPrimitiveShape(el, local, out, err, ctx, effectiveClip);
}

} // namespace

bool loadSvgPainterPath(const QString& xml, QPainterPath& out, QString* error_message,
                        const QString& document_base_dir, QStringList* warnings)
{
    QDomDocument doc;
    QString err;
    int line = 0;
    int col = 0;
    if (!doc.setContent(xml, false, &err, &line, &col)) {
        if (error_message)
            *error_message =
                QStringLiteral("SVG XML: %1 @ %2:%3").arg(err).arg(line).arg(col);
        return false;
    }

    QDomElement root = doc.documentElement();
    if (root.tagName().compare(QLatin1String("svg"), Qt::CaseInsensitive) != 0) {
        if (error_message)
            *error_message = QStringLiteral("Oczekiwano korzenia <svg>");
        return false;
    }

    QHash<QString, QDomElement> ids;
    collectIds(root, ids);

    WalkCtx ctx;
    ctx.ids = &ids;
    ctx.document_base_dir = document_base_dir;
    ctx.warnings = warnings;

    QString inner_err;

    QTransform root_xf = svgViewportTransform(root);
    root_xf *= parseTransformChain(QStringView(root.attribute(QStringLiteral("transform"))));

    std::optional<QPainterPath> root_clip_buf;
    const QPainterPath* root_clip = nullptr;
    resolveVisibilityClip(root, root_xf, nullptr, root_clip_buf, root_clip, &inner_err, ctx);
    if (!inner_err.isEmpty()) {
        if (error_message)
            *error_message = inner_err;
        return false;
    }

    for (QDomNode n = root.firstChild(); !n.isNull(); n = n.nextSibling()) {
        if (!n.isElement())
            continue;
        renderSubtree(n.toElement(), root_xf, out, &inner_err, ctx, root_clip);
        if (!inner_err.isEmpty()) {
            if (error_message)
                *error_message = inner_err;
            return false;
        }
    }

    if (!out.isEmpty())
        out = QTransform::fromScale(kMmPerPlotUnit, kMmPerPlotUnit).map(out);

    return true;
}

} // namespace inkcut
