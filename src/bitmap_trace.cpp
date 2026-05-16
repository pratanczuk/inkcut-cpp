// SPDX-License-Identifier: GPL-3.0-or-later

#include "bitmap_trace.hpp"

#include "path_utils.hpp"

#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QLineF>
#include <QPoint>
#include <QRectF>
#include <QVector>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

#ifdef INKCUT_HAVE_POTRACE
#include <potracelib.h>
#endif

namespace inkcut {

bool isBitmapTracingAvailable()
{
#ifdef INKCUT_HAVE_POTRACE
    return true;
#else
    return false;
#endif
}

#ifdef INKCUT_HAVE_POTRACE

namespace {

struct PotraceProgressCtx {
    TraceProgressFn fn;
    int base = 10;
    int span = 80;
};

void potraceProgressCallback(double prog, void* privdata)
{
    auto* ctx = static_cast<PotraceProgressCtx*>(privdata);
    if (!ctx || !ctx->fn)
        return;
    const int pct = ctx->base + static_cast<int>(prog * ctx->span + 0.5);
    ctx->fn(qBound(0, pct, 99));
}

void freeBitmap(potrace_bitmap_t* bm)
{
    if (!bm)
        return;
    delete[] bm->map;
    delete bm;
}

constexpr int potraceWordBits()
{
    return static_cast<int>(8 * sizeof(potrace_word));
}

int potraceWordsPerRow(int width)
{
    const int bits = potraceWordBits();
    return (width + bits - 1) / bits;
}

void bitmapSetBlackPixel(potrace_bitmap_t* bm, int x, int y)
{
    const int bits = potraceWordBits();
    const size_t word_index = static_cast<size_t>(y) * static_cast<size_t>(bm->dy)
                            + static_cast<size_t>(x / bits);
    const int bit = bits - 1 - (x % bits);
    bm->map[word_index] |= (potrace_word(1) << bit);
}

QImage compositeOnWhite(const QImage& src)
{
    if (!src.hasAlphaChannel())
        return src;
    QImage flat(src.size(), QImage::Format_RGB32);
    flat.fill(Qt::white);
    QPainter painter(&flat);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.drawImage(0, 0, src);
    return flat;
}

bool isMostlyBilevel(const QImage& gray)
{
    int hist[256] = {};
    const int total = gray.width() * gray.height();
    if (total <= 0)
        return true;

    for (int y = 0; y < gray.height(); ++y) {
        const uchar* row = gray.constScanLine(y);
        for (int x = 0; x < gray.width(); ++x)
            ++hist[row[x]];
    }

    int significant = 0;
    for (int i = 0; i < 256; ++i) {
        if (hist[i] * 1000 > total)
            ++significant;
    }
    return significant <= 6;
}

bool imageIsBilevel(const QImage& img)
{
    switch (img.format()) {
    case QImage::Format_Mono:
    case QImage::Format_MonoLSB:
    case QImage::Format_Indexed8:
        return img.colorCount() <= 2;
    default:
        break;
    }
    if (img.depth() == 1)
        return true;
    QImage gray = img.format() == QImage::Format_Grayscale8
                      ? img
                      : img.convertToFormat(QImage::Format_Grayscale8);
    return !gray.isNull() && isMostlyBilevel(gray);
}

/// Skalowanie: zmniejszanie ogromnych plików; opcjonalnie powiększenie przed progowaniem (mkbitmap -s).
QImage scaleForTracing(const QImage& img, bool bilevel, const BitmapTraceOptions& opt)
{
    const int max_side = qMax(img.width(), img.height());
    const Qt::TransformationMode mode =
        bilevel ? Qt::FastTransformation : Qt::SmoothTransformation;

    if (max_side > 3200) {
        return img.scaled(3200, 3200, Qt::KeepAspectRatio, mode);
    }

    int upscale = 1;
    if (opt.scale_factor >= 1) {
        if (!bilevel)
            upscale = qBound(1, opt.scale_factor, 4);
    } else if (!bilevel && max_side < 300) {
        upscale = 2;
    }

    if (upscale > 1) {
        return img.scaled(img.width() * upscale, img.height() * upscale, Qt::IgnoreAspectRatio,
                          Qt::SmoothTransformation);
    }
    return img;
}

/// Rozmycie pudełkowe (radius w pikselach), Format_Grayscale8.
QImage boxBlur(const QImage& src, int radius)
{
    const int w = src.width();
    const int h = src.height();
    if (w < 1 || h < 1 || radius < 1)
        return src;

    const int r = radius;
    const int win = 2 * r + 1;
    QImage horiz(w, h, QImage::Format_Grayscale8);

    for (int y = 0; y < h; ++y) {
        const uchar* in = src.constScanLine(y);
        uchar* out = horiz.scanLine(y);
        int sum = 0;
        for (int x = -r; x <= r; ++x)
            sum += in[qBound(0, x, w - 1)];
        out[0] = static_cast<uchar>(sum / win);
        for (int x = 1; x < w; ++x) {
            sum += in[qMin(w - 1, x + r)] - in[qMax(0, x - r - 1)];
            out[x] = static_cast<uchar>(sum / win);
        }
    }

    QImage out(w, h, QImage::Format_Grayscale8);
    for (int x = 0; x < w; ++x) {
        int sum = 0;
        for (int y = -r; y <= r; ++y)
            sum += horiz.constScanLine(qBound(0, y, h - 1))[x];
        out.scanLine(0)[x] = static_cast<uchar>(sum / win);
        for (int y = 1; y < h; ++y) {
            sum += horiz.constScanLine(qMin(h - 1, y + r))[x] -
                   horiz.constScanLine(qMax(0, y - r - 1))[x];
            out.scanLine(y)[x] = static_cast<uchar>(sum / win);
        }
    }
    return out;
}

/// Filtr górnoprzepustowy jak mkbitmap (-f): tłumi tło, zostawia kontury.
QImage highpassFilter(const QImage& gray, int radius)
{
    const QImage blurred = boxBlur(gray, radius);
    QImage hp(gray.size(), QImage::Format_Grayscale8);
    for (int y = 0; y < gray.height(); ++y) {
        const uchar* g = gray.constScanLine(y);
        const uchar* b = blurred.constScanLine(y);
        uchar* o = hp.scanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            const int v = static_cast<int>(g[x]) - static_cast<int>(b[x]) + 128;
            o[x] = static_cast<uchar>(qBound(0, v, 255));
        }
    }
    return hp;
}

int otsuThreshold(const QImage& gray)
{
    int hist[256] = {};
    const int total = gray.width() * gray.height();
    if (total <= 0)
        return 128;

    for (int y = 0; y < gray.height(); ++y) {
        const uchar* row = gray.constScanLine(y);
        for (int x = 0; x < gray.width(); ++x)
            ++hist[row[x]];
    }

    qint64 sum = 0;
    for (int i = 0; i < 256; ++i)
        sum += qint64(i) * hist[i];

    qint64 sum_b = 0;
    int w_b = 0;
    double max_var = -1.0;
    int best = 128;

    for (int t = 0; t < 256; ++t) {
        w_b += hist[t];
        if (w_b == 0)
            continue;
        const int w_f = total - w_b;
        if (w_f == 0)
            break;
        sum_b += qint64(t) * hist[t];
        const double m_b = double(sum_b) / w_b;
        const double m_f = double(sum - sum_b) / w_f;
        const double var_between = double(w_b) * w_f * (m_b - m_f) * (m_b - m_f);
        if (var_between > max_var) {
            max_var = var_between;
            best = t;
        }
    }
    return best;
}

QImage despeckleBinary(const QImage& binary, int min_area);

bool isBlackPixel(uchar v)
{
    return v < 128;
}

/// Rozszerzenie czerni (wypełnia cienkie białe szczeliny).
QImage dilateBlack(const QImage& binary, int radius)
{
    const int w = binary.width();
    const int h = binary.height();
    if (radius < 1 || w < 1 || h < 1)
        return binary;

    QImage out(w, h, QImage::Format_Grayscale8);
    for (int y = 0; y < h; ++y) {
        uchar* row = out.scanLine(y);
        for (int x = 0; x < w; ++x) {
            bool black = false;
            for (int dy = -radius; dy <= radius && !black; ++dy) {
                const int yy = qBound(0, y + dy, h - 1);
                const uchar* in_row = binary.constScanLine(yy);
                for (int dx = -radius; dx <= radius; ++dx) {
                    const int xx = qBound(0, x + dx, w - 1);
                    if (isBlackPixel(in_row[xx])) {
                        black = true;
                        break;
                    }
                }
            }
            row[x] = black ? 0 : 255;
        }
    }
    return out;
}

/// Erozja czerni (po dylatacji przywraca grubość, domykając szczeliny).
QImage erodeBlack(const QImage& binary, int radius)
{
    const int w = binary.width();
    const int h = binary.height();
    if (radius < 1 || w < 1 || h < 1)
        return binary;

    QImage out(w, h, QImage::Format_Grayscale8);
    for (int y = 0; y < h; ++y) {
        uchar* row = out.scanLine(y);
        for (int x = 0; x < w; ++x) {
            bool all_black = true;
            for (int dy = -radius; dy <= radius && all_black; ++dy) {
                const int yy = qBound(0, y + dy, h - 1);
                const uchar* in_row = binary.constScanLine(yy);
                for (int dx = -radius; dx <= radius; ++dx) {
                    const int xx = qBound(0, x + dx, w - 1);
                    if (!isBlackPixel(in_row[xx])) {
                        all_black = false;
                        break;
                    }
                }
            }
            row[x] = all_black ? 0 : 255;
        }
    }
    return out;
}

QImage morphCloseBlack(const QImage& binary, int radius)
{
    if (radius < 1)
        return binary;
    return erodeBlack(dilateBlack(binary, radius), radius);
}

/// Białe obszary wewnątrz sylwetki (nie stykające się z brzegiem obrazu) → czernia.
QImage fillInteriorWhiteHoles(const QImage& binary, int max_area)
{
    if (binary.isNull() || max_area <= 0)
        return binary;

    const int w = binary.width();
    const int h = binary.height();
    QImage out = binary;
    QVector<int> labels(w * h, 0);
    int next_label = 0;
    QVector<int> comp_sizes;
    QVector<char> touches_border;

    const auto index = [w](int x, int y) { return y * w + x; };
    const auto is_white = [&](int x, int y) {
        return !isBlackPixel(binary.constScanLine(y)[x]);
    };

    QVector<QPoint> stack;
    stack.reserve(4096);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (!is_white(x, y) || labels[index(x, y)] != 0)
                continue;

            const int label = ++next_label;
            int count = 0;
            bool border = false;
            stack.clear();
            stack.push_back(QPoint(x, y));
            labels[index(x, y)] = label;

            while (!stack.isEmpty()) {
                const QPoint p = stack.takeLast();
                ++count;
                if (p.x() == 0 || p.y() == 0 || p.x() == w - 1 || p.y() == h - 1)
                    border = true;
                static const int dx[] = {1, -1, 0, 0};
                static const int dy[] = {0, 0, 1, -1};
                for (int k = 0; k < 4; ++k) {
                    const int nx = p.x() + dx[k];
                    const int ny = p.y() + dy[k];
                    if (nx < 0 || ny < 0 || nx >= w || ny >= h)
                        continue;
                    const int ni = index(nx, ny);
                    if (!is_white(nx, ny) || labels[ni] != 0)
                        continue;
                    labels[ni] = label;
                    stack.push_back(QPoint(nx, ny));
                }
            }

            comp_sizes.push_back(count);
            touches_border.push_back(border ? 1 : 0);
        }
    }

    for (int y = 0; y < h; ++y) {
        uchar* row = out.scanLine(y);
        for (int x = 0; x < w; ++x) {
            const int lab = labels[index(x, y)];
            if (lab <= 0)
                continue;
            if (touches_border[lab - 1])
                continue;
            if (comp_sizes[lab - 1] <= max_area)
                row[x] = 0;
        }
    }
    return out;
}

int autoHighpassRadius(const QImage& gray)
{
    return qBound(1, qMax(gray.width(), gray.height()) / 256, 8);
}

int autoDespeckleArea(bool bilevel)
{
    return bilevel ? 3 : 8;
}

int autoTurdsize(const QImage& prepared)
{
    if (prepared.width() <= 0 || prepared.height() <= 0)
        return 2;
    const int side = qMax(prepared.width(), prepared.height());
    if (side > 400)
        return qMin(10, qMax(2, side / 200));
    return 2;
}

/// Przygotowanie jak PBM + mkbitmap: jasne tło, czarne kształty do śledzenia.
QImage prepareImageForTrace(const QImage& src, const BitmapTraceOptions& opt)
{
    QImage img = compositeOnWhite(src);
    if (img.isNull())
        return {};

    const bool bilevel = imageIsBilevel(img);
    img = scaleForTracing(img, bilevel, opt);
    QImage gray = img.convertToFormat(QImage::Format_Grayscale8);
    if (gray.isNull())
        return {};

    qint64 sum = 0;
    const int pixels = gray.width() * gray.height();
    for (int y = 0; y < gray.height(); ++y) {
        const uchar* row = gray.constScanLine(y);
        for (int x = 0; x < gray.width(); ++x)
            sum += row[x];
    }
    const bool dark_background = pixels > 0 && sum / pixels < 128;
    if (dark_background || opt.force_invert) {
        for (int y = 0; y < gray.height(); ++y) {
            uchar* row = gray.scanLine(y);
            for (int x = 0; x < gray.width(); ++x)
                row[x] = 255 - row[x];
        }
    }

    QImage work = gray;
    if (!bilevel && opt.highpass_radius != 0) {
        const int radius = opt.highpass_radius > 0 ? opt.highpass_radius : autoHighpassRadius(gray);
        work = highpassFilter(gray, radius);
    }

    int threshold = 128;
    if (opt.threshold >= 0.0 && opt.threshold <= 1.0) {
        threshold = qBound(0, static_cast<int>(opt.threshold * 255.0 + 0.5), 255);
    } else if (!bilevel) {
        const int thr = otsuThreshold(work);
        const int mk_thr = qBound(0, static_cast<int>(0.48 * 255.0 + 0.5), 255);
        threshold = (thr + mk_thr) / 2;
    }

    QImage binary(work.size(), QImage::Format_Grayscale8);
    for (int y = 0; y < work.height(); ++y) {
        const uchar* in = work.constScanLine(y);
        uchar* out = binary.scanLine(y);
        for (int x = 0; x < work.width(); ++x)
            out[x] = in[x] < threshold ? 0 : 255;
    }
    const int speck = opt.despeckle_min_area >= 0 ? opt.despeckle_min_area
                                                  : autoDespeckleArea(bilevel);
    QImage cleaned = despeckleBinary(binary, speck);
    if (!cleaned.isNull())
        binary = cleaned;

    if (opt.gap_close_radius > 0)
        binary = morphCloseBlack(binary, qBound(1, opt.gap_close_radius, 16));

    if (opt.fill_holes_max_area > 0)
        binary = fillInteriorWhiteHoles(binary, opt.fill_holes_max_area);

    return binary;
}

/// Usuwa drobne plamki (szum skanera / antyaliasing) przed potrace.
QImage despeckleBinary(const QImage& binary, int min_area)
{
    if (binary.isNull())
        return {};
    if (min_area <= 0)
        return binary;
    if (min_area <= 1)
        return {};

    const int w = binary.width();
    const int h = binary.height();
    QImage out = binary;
    QVector<int> labels(w * h, 0);
    int next_label = 1;
    QVector<int> comp_sizes;
    comp_sizes.reserve(256);

    const auto index = [w](int x, int y) { return y * w + x; };
    const auto is_black = [&](int x, int y) {
        return binary.constScanLine(y)[x] < 128;
    };

    QVector<QPoint> stack;
    stack.reserve(4096);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (!is_black(x, y) || labels[index(x, y)] != 0)
                continue;

            const int label = next_label++;
            int count = 0;
            stack.clear();
            stack.push_back(QPoint(x, y));
            labels[index(x, y)] = label;

            while (!stack.isEmpty()) {
                const QPoint p = stack.takeLast();
                ++count;
                static const int dx[] = {1, -1, 0, 0};
                static const int dy[] = {0, 0, 1, -1};
                for (int k = 0; k < 4; ++k) {
                    const int nx = p.x() + dx[k];
                    const int ny = p.y() + dy[k];
                    if (nx < 0 || ny < 0 || nx >= w || ny >= h)
                        continue;
                    const int ni = index(nx, ny);
                    if (!is_black(nx, ny) || labels[ni] != 0)
                        continue;
                    labels[ni] = label;
                    stack.push_back(QPoint(nx, ny));
                }
            }

            comp_sizes.resize(next_label);
            comp_sizes[label - 1] = count;
        }
    }

    for (int y = 0; y < h; ++y) {
        uchar* row = out.scanLine(y);
        for (int x = 0; x < w; ++x) {
            const int lab = labels[index(x, y)];
            if (lab > 0 && comp_sizes[lab - 1] < min_area)
                row[x] = 255;
        }
    }
    return out;
}

potrace_bitmap_t* imageToBitmap(const QImage& binary, QString* err)
{
    if (binary.format() != QImage::Format_Grayscale8 || binary.isNull()) {
        if (err)
            *err = QStringLiteral("Nie można przygotować obrazu do śledzenia.");
        return nullptr;
    }

    const int w = binary.width();
    const int h = binary.height();
    auto* bm = new potrace_bitmap_t;
    bm->w = w;
    bm->h = h;
    bm->dy = potraceWordsPerRow(w);
    const size_t map_words = static_cast<size_t>(bm->dy) * static_cast<size_t>(h);
    bm->map = new potrace_word[map_words]();
    if (!bm->map) {
        delete bm;
        if (err)
            *err = QStringLiteral("Brak pamięci na bitmapę potrace.");
        return nullptr;
    }

    for (int y = 0; y < h; ++y) {
        const uchar* row = binary.constScanLine(y);
        for (int x = 0; x < w; ++x) {
            if (row[x] < 128)
                bitmapSetBlackPixel(bm, x, y);
        }
    }
    return bm;
}

void curveBounds(const potrace_curve_t& curve, double& min_x, double& min_y, double& max_x,
                 double& max_y, double& max_seg_len, QPointF& last_pt, bool& have_last)
{
    const int m = curve.n;
    if (m <= 0 || !curve.c || !curve.tag)
        return;

    auto visit = [&](double x, double y) {
        min_x = std::min(min_x, x);
        min_y = std::min(min_y, y);
        max_x = std::max(max_x, x);
        max_y = std::max(max_y, y);
        if (have_last) {
            const double dx = x - last_pt.x();
            const double dy = y - last_pt.y();
            max_seg_len = std::max(max_seg_len, std::hypot(dx, dy));
        }
        last_pt = QPointF(x, y);
        have_last = true;
    };

    const potrace_dpoint_t (*c)[3] = curve.c;
    visit(c[m - 1][2].x, c[m - 1][2].y);
    for (int i = 0; i < m; ++i) {
        if (curve.tag[i] == POTRACE_CORNER) {
            visit(c[i][1].x, c[i][1].y);
            visit(c[i][2].x, c[i][2].y);
        } else {
            visit(c[i][0].x, c[i][0].y);
            visit(c[i][1].x, c[i][1].y);
            visit(c[i][2].x, c[i][2].y);
        }
    }
}

bool potraceTreeIsReasonable(potrace_path_t* p, int img_w, int img_h)
{
    if (!p || p->curve.n <= 0)
        return false;
    if (std::abs(p->area) < 4)
        return false;

    double min_x = 1e300;
    double min_y = 1e300;
    double max_x = -1e300;
    double max_y = -1e300;
    double max_seg = 0.0;
    QPointF last;
    bool have_last = false;

    std::function<void(potrace_path_t*)> walk;
    walk = [&](potrace_path_t* node) {
        for (; node; node = node->sibling) {
            curveBounds(node->curve, min_x, min_y, max_x, max_y, max_seg, last, have_last);
            for (potrace_path_t* ch = node->childlist; ch; ch = ch->sibling) {
                curveBounds(ch->curve, min_x, min_y, max_x, max_y, max_seg, last, have_last);
                walk(ch->childlist);
            }
        }
    };
    walk(p);

    const double diag = std::hypot(double(img_w), double(img_h));
    if (max_seg > diag * 1.25)
        return false;
    if (max_x < -img_w * 0.1 || min_x > img_w * 1.1 || max_y < -img_h * 0.1 || min_y > img_h * 1.1)
        return false;
    if (max_x - min_x < 0.5 && max_y - min_y < 0.5)
        return false;
    return true;
}

double subpathMaxSegmentLength(const QPainterPath& subpath)
{
    double max_len = 0.0;
    QPointF last;
    bool have = false;
    for (int i = 0; i < subpath.elementCount(); ++i) {
        const auto e = subpath.elementAt(i);
        if (e.type == QPainterPath::MoveToElement) {
            last = QPointF(e.x, e.y);
            have = true;
            continue;
        }
        if (!have)
            continue;
        const QPointF p(e.x, e.y);
        if (e.type == QPainterPath::LineToElement || e.type == QPainterPath::CurveToElement) {
            max_len = std::max(max_len, QLineF(last, p).length());
            last = p;
        }
    }
    return max_len;
}

QPainterPath sanitizeTracedPath(const QPainterPath& path, int img_w, int img_h)
{
    const double max_seg = std::hypot(double(img_w), double(img_h)) * 0.85;
    const QRectF bounds(0, 0, img_w, img_h);
    const QRectF margin = bounds.adjusted(-2, -2, 2, 2);

    QPainterPath out;
    for (const QPainterPath& part : splitPainterPath(path)) {
        if (part.isEmpty())
            continue;
        const QRectF bb = part.boundingRect();
        if (!margin.intersects(bb))
            continue;
        if (subpathMaxSegmentLength(part) > max_seg)
            continue;
        out.addPath(part);
    }
    return out;
}

QString pathToSvgData(const QPainterPath& path)
{
    QString d;
    QPointF pos;
    int i = 0;
    while (i < path.elementCount()) {
        const QPainterPath::Element e = path.elementAt(i);
        if (e.type == QPainterPath::MoveToElement) {
            d += QStringLiteral("M %1 %2 ")
                     .arg(e.x, 0, 'g', 6)
                     .arg(e.y, 0, 'g', 6);
            pos = QPointF(e.x, e.y);
            ++i;
        } else if (e.type == QPainterPath::LineToElement) {
            d += QStringLiteral("L %1 %2 ")
                     .arg(e.x, 0, 'g', 6)
                     .arg(e.y, 0, 'g', 6);
            pos = QPointF(e.x, e.y);
            ++i;
        } else if (e.type == QPainterPath::CurveToElement && i + 1 < path.elementCount()) {
            const auto e1 = path.elementAt(i + 1);
            if (e1.type != QPainterPath::CurveToDataElement) {
                ++i;
                continue;
            }
            if (i + 2 < path.elementCount()
                && path.elementAt(i + 2).type == QPainterPath::CurveToDataElement) {
                const auto e2 = path.elementAt(i + 2);
                d += QStringLiteral("C %1 %2 %3 %4 %5 %6 ")
                         .arg(e.x, 0, 'g', 6)
                         .arg(e.y, 0, 'g', 6)
                         .arg(e1.x, 0, 'g', 6)
                         .arg(e1.y, 0, 'g', 6)
                         .arg(e2.x, 0, 'g', 6)
                         .arg(e2.y, 0, 'g', 6);
                pos = QPointF(e2.x, e2.y);
                i += 3;
            } else {
                d += QStringLiteral("Q %1 %2 %3 %4 ")
                         .arg(e.x, 0, 'g', 6)
                         .arg(e.y, 0, 'g', 6)
                         .arg(e1.x, 0, 'g', 6)
                         .arg(e1.y, 0, 'g', 6);
                pos = QPointF(e1.x, e1.y);
                i += 2;
            }
        } else {
            ++i;
        }
    }
    return d.trimmed();
}

QString curveToSvgD(const potrace_curve_t& curve)
{
    const int m = curve.n;
    if (m <= 0 || !curve.c || !curve.tag)
        return {};

    const potrace_dpoint_t (*c)[3] = curve.c;
    QString d;
    d += QStringLiteral("M %1 %2 ")
             .arg(c[m - 1][2].x, 0, 'g', 8)
             .arg(c[m - 1][2].y, 0, 'g', 8);

    for (int i = 0; i < m; ++i) {
        if (curve.tag[i] == POTRACE_CORNER) {
            d += QStringLiteral("L %1 %2 L %3 %4 ")
                     .arg(c[i][1].x, 0, 'g', 8)
                     .arg(c[i][1].y, 0, 'g', 8)
                     .arg(c[i][2].x, 0, 'g', 8)
                     .arg(c[i][2].y, 0, 'g', 8);
        } else {
            d += QStringLiteral("C %1 %2 %3 %4 %5 %6 ")
                     .arg(c[i][0].x, 0, 'g', 8)
                     .arg(c[i][0].y, 0, 'g', 8)
                     .arg(c[i][1].x, 0, 'g', 8)
                     .arg(c[i][1].y, 0, 'g', 8)
                     .arg(c[i][2].x, 0, 'g', 8)
                     .arg(c[i][2].y, 0, 'g', 8);
        }
    }
    d += QLatin1Char('z');
    return d;
}

void appendPotraceCurve(const potrace_curve_t& curve, QPainterPath& out)
{
    const int m = curve.n;
    if (m <= 0 || !curve.c || !curve.tag)
        return;

    const potrace_dpoint_t (*c)[3] = curve.c;
    // Współrzędne potrace są zawsze absolutne (jak w backend_svg.c).
    out.moveTo(c[m - 1][2].x, c[m - 1][2].y);

    for (int i = 0; i < m; ++i) {
        if (curve.tag[i] == POTRACE_CORNER) {
            out.lineTo(c[i][1].x, c[i][1].y);
            out.lineTo(c[i][2].x, c[i][2].y);
        } else {
            out.cubicTo(c[i][0].x, c[i][0].y, c[i][1].x, c[i][1].y, c[i][2].x, c[i][2].y);
        }
    }
    out.closeSubpath();
}

void appendPotracePath(potrace_path_t* p, QPainterPath& out)
{
    if (!p)
        return;
    appendPotraceCurve(p->curve, out);
    for (potrace_path_t* q = p->childlist; q; q = q->sibling)
        appendPotraceCurve(q->curve, out);
    for (potrace_path_t* q = p->childlist; q; q = q->sibling)
        appendPotracePath(q->childlist, out);
}

void appendPotracePathToSvgD(potrace_path_t* tree, QString& d)
{
    for (potrace_path_t* node = tree; node; node = node->sibling) {
        d += curveToSvgD(node->curve);
        d += QLatin1Char(' ');
        for (potrace_path_t* q = node->childlist; q; q = q->sibling) {
            d += curveToSvgD(q->curve);
            d += QLatin1Char(' ');
        }
        for (potrace_path_t* q = node->childlist; q; q = q->sibling)
            appendPotracePathToSvgD(q->childlist, d);
    }
}

void appendPlistToSvgD(potrace_path_t* plist, QString& d)
{
    for (potrace_path_t* p = plist; p; p = p->next) {
        d += curveToSvgD(p->curve);
        d += QLatin1Char(' ');
        for (potrace_path_t* q = p->childlist; q; q = q->sibling) {
            d += curveToSvgD(q->curve);
            d += QLatin1Char(' ');
        }
        for (potrace_path_t* q = p->childlist; q; q = q->sibling)
            appendPotracePathToSvgD(q->childlist, d);
    }
}

potrace_state_t* tracePreparedBitmap(const QImage& prepared, const BitmapTraceOptions& opt,
                                     QString* err, const TraceProgressFn& progress)
{
    potrace_bitmap_t* bm = imageToBitmap(prepared, err);
    if (!bm)
        return nullptr;

    potrace_param_t* param = potrace_param_default();
    if (!param) {
        freeBitmap(bm);
        if (err)
            *err = QStringLiteral("potrace_param_default nie powiódł się.");
        return nullptr;
    }

    param->turdsize =
        opt.turdsize >= 0 ? opt.turdsize : autoTurdsize(prepared);
    param->turnpolicy = POTRACE_TURNPOLICY_MINORITY;
    param->alphamax = 1.0;
    param->opticurve = 1;
    param->opttolerance = 0.2;

    PotraceProgressCtx prog_ctx;
    prog_ctx.fn = progress;
    if (progress) {
        param->progress.callback = potraceProgressCallback;
        param->progress.data = &prog_ctx;
        param->progress.min = 0.0;
        param->progress.max = 1.0;
        param->progress.epsilon = 0.01;
    }

    potrace_state_t* st = potrace_trace(param, bm);
    potrace_param_free(param);
    freeBitmap(bm);
    return st;
}

} // namespace

QImage previewBitmapForTrace(const QImage& source, const BitmapTraceOptions& options)
{
    if (source.isNull())
        return {};
    return prepareImageForTrace(source, options);
}

#else

QImage previewBitmapForTrace(const QImage& source, const BitmapTraceOptions& options)
{
    Q_UNUSED(source);
    Q_UNUSED(options);
    return {};
}

#endif

bool traceBitmapToSvg(const QImage& image, QString& svg_xml_out, QString* error_message,
                      const TraceProgressFn& progress, const BitmapTraceOptions& options)
{
    svg_xml_out.clear();
    if (image.isNull()) {
        if (error_message)
            *error_message = QStringLiteral("Pusty obraz.");
        return false;
    }

#ifndef INKCUT_HAVE_POTRACE
    if (error_message) {
        *error_message =
            QStringLiteral("Brak libpotrace — zainstaluj libpotrace-dev i przebuduj projekt.");
    }
    return false;
#else
    if (progress)
        progress(2);

    const QImage prepared = prepareImageForTrace(image, options);
    if (prepared.isNull()) {
        if (error_message)
            *error_message = QStringLiteral("Nie można przygotować obrazu.");
        return false;
    }

    if (progress)
        progress(8);

    QString err;
    potrace_state_t* st = tracePreparedBitmap(prepared, options, &err, progress);
    if (!st || st->status != POTRACE_STATUS_OK) {
        if (st)
            potrace_state_free(st);
        if (error_message)
            *error_message =
                err.isEmpty() ? QStringLiteral("potrace_trace nie powiódł się.") : err;
        return false;
    }

    if (progress)
        progress(92);

    QString path_d;
    appendPlistToSvgD(st->plist, path_d);
    potrace_state_free(st);
    if (path_d.isEmpty()) {
        if (error_message)
            *error_message = QStringLiteral("Potrace nie znalazł kształtów do wycięcia.");
        return false;
    }

    const int w = prepared.width();
    const int h = prepared.height();
    svg_xml_out = QStringLiteral(
                      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                      "<svg xmlns=\"http://www.w3.org/2000/svg\" "
                      "viewBox=\"0 0 %1 %2\" width=\"%1\" height=\"%2\">\n"
                      "<path fill=\"#000000\" stroke=\"none\" fill-rule=\"evenodd\" d=\"%3\"/>\n"
                      "</svg>\n")
                      .arg(w)
                      .arg(h)
                      .arg(path_d);

    if (progress)
        progress(100);
    return true;
#endif
}

bool traceBitmapToPath(const QImage& image, QPainterPath& out, QString* error_message,
                       const TraceProgressFn& progress, const BitmapTraceOptions& options)
{
    out = QPainterPath();
    if (image.isNull()) {
        if (error_message)
            *error_message = QStringLiteral("Pusty obraz.");
        return false;
    }

#ifndef INKCUT_HAVE_POTRACE
    if (error_message) {
        *error_message =
            QStringLiteral("Brak libpotrace — zainstaluj libpotrace-dev i przebuduj projekt.");
    }
    return false;
#else
    if (progress)
        progress(2);

    const QImage prepared = prepareImageForTrace(image, options);
    if (prepared.isNull()) {
        if (error_message)
            *error_message = QStringLiteral("Nie można przygotować obrazu.");
        return false;
    }

    if (progress)
        progress(8);

    QString err;
    potrace_state_t* st = tracePreparedBitmap(prepared, options, &err, progress);
    if (!st || st->status != POTRACE_STATUS_OK) {
        if (st)
            potrace_state_free(st);
        if (error_message)
            *error_message =
                err.isEmpty() ? QStringLiteral("potrace_trace nie powiódł się.") : err;
        return false;
    }

    if (progress)
        progress(92);

    for (potrace_path_t* p = st->plist; p; p = p->next)
        appendPotracePath(p, out);

    potrace_state_free(st);

    if (progress)
        progress(100);

    return !out.isEmpty();
#endif
}

QPixmap renderBitmapTracePreview(const QImage& source, const BitmapTraceOptions& options,
                                 const QSize& target_size)
{
    if (source.isNull() || target_size.width() < 8 || target_size.height() < 8)
        return {};

    QPainterPath path;
    if (!traceBitmapToPath(source, path, nullptr, nullptr, options))
        return {};

    const QRectF br = path.boundingRect();
    if (br.isEmpty())
        return {};

    QPixmap pm(target_size);
    pm.fill(Qt::white);
    QPainter painter(&pm);
    painter.setRenderHint(QPainter::Antialiasing, true);

    constexpr double margin = 10.0;
    const double sx = (target_size.width() - 2.0 * margin) / br.width();
    const double sy = (target_size.height() - 2.0 * margin) / br.height();
    const double scale = qMin(sx, sy);

    QTransform xf;
    xf.translate(margin, margin);
    xf.scale(scale, scale);
    xf.translate(-br.left(), -br.top());
    painter.setTransform(xf);
    QPainterPath fill_path = path;
    fill_path.setFillRule(Qt::OddEvenFill);
    painter.setPen(Qt::NoPen);
    painter.fillPath(fill_path, Qt::black);
    painter.end();
    return pm;
}

} // namespace inkcut
