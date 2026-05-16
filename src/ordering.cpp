// SPDX-License-Identifier: GPL-3.0-or-later

#include "ordering.hpp"

#include "path_utils.hpp"

#include <QVector2D>
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace inkcut {

namespace {

QVector2D elementToVec(const QPainterPath::Element& e)
{
    return QVector2D(static_cast<float>(e.x), static_cast<float>(e.y));
}

QVector2D startPoint(const QPainterPath& path)
{
    return elementToVec(path.elementAt(0));
}

QVector2D endPoint(const QPainterPath& path)
{
    return elementToVec(path.elementAt(path.elementCount() - 1));
}

double curvePosHilbert(QVector2D p, QVector2D p0, double s)
{
    double ss = s * 0.5;
    p.setX(p.x() - p0.x());
    p.setY(p.y() - p0.y());
    double x = p.x();
    double y = p.y();
    qint64 result = 0;
    const int STEPS = 32;
    for (int i = 0; i < STEPS; ++i) {
        int bits = 0;
        if (x > ss) {
            bits = 1;
            x -= ss;
            if (y > ss) {
                bits = 2;
                y -= ss;
            }
        } else {
            if (y > ss) {
                bits = 3;
                const double nx = ss - (y - ss);
                const double ny = ss - x;
                x = nx;
                y = ny;
            } else {
                std::swap(x, y);
            }
        }
        result = (result << 2) + bits;
        ss *= 0.5;
    }
    return static_cast<double>(result);
}

double curvePosZ(QVector2D p, QVector2D p0, double s)
{
    p.setX(p.x() - p0.x());
    p.setY(p.y() - p0.y());
    double x = p.x();
    double y = p.y();
    qint64 result = 0;
    const int STEPS = 32;
    for (int i = 0; i < STEPS; ++i) {
        int bits = 0;
        if (y > s) {
            y -= s;
            bits += 2;
        }
        if (x > s) {
            x -= s;
            bits += 1;
        }
        result = (result << 2) + bits;
        s *= 0.5;
    }
    return static_cast<double>(result);
}

struct KdNode {
    QVector2D position;
    int id = -1;
    int count = 0;
    KdNode* left = nullptr;
    KdNode* right = nullptr;
    KdNode* parent = nullptr;
};

static int itemCount(KdNode* n)
{
    return n ? n->count : 0;
}

KdNode* recursiveBuild(std::vector<KdNode*>& items, int depth, KdNode* parent)
{
    if (items.empty())
        return nullptr;
    if (depth & 1)
        std::sort(items.begin(), items.end(),
                  [](KdNode* a, KdNode* b) { return a->position.y() < b->position.y(); });
    else
        std::sort(items.begin(), items.end(),
                  [](KdNode* a, KdNode* b) { return a->position.x() < b->position.x(); });

    const int m = static_cast<int>(items.size()) / 2;
    KdNode* pivot = items[m];
    std::vector<KdNode*> left(items.begin(), items.begin() + m);
    std::vector<KdNode*> right(items.begin() + m + 1, items.end());

    pivot->parent = parent;
    pivot->left = recursiveBuild(left, depth + 1, pivot);
    pivot->right = recursiveBuild(right, depth + 1, pivot);
    pivot->count = 1 + itemCount(pivot->left) + itemCount(pivot->right);
    return pivot;
}

struct KdTree {
    QVector2D minp;
    QVector2D maxp;
    std::vector<KdNode> nodes;
    KdNode* root = nullptr;

    explicit KdTree(const std::vector<QVector2D>& points)
    {
        if (points.empty())
            return;
        minp = maxp = points.front();
        for (const QVector2D& p : points) {
            minp.setX(std::min(minp.x(), p.x()));
            minp.setY(std::min(minp.y(), p.y()));
            maxp.setX(std::max(maxp.x(), p.x()));
            maxp.setY(std::max(maxp.y(), p.y()));
        }
        nodes.reserve(points.size());
        for (int i = 0; i < points.size(); ++i) {
            KdNode n;
            n.position = points[i];
            n.id = i;
            nodes.push_back(std::move(n));
        }
        std::vector<KdNode*> ptrs;
        ptrs.reserve(nodes.size());
        for (auto& n : nodes)
            ptrs.push_back(&n);
        root = recursiveBuild(ptrs, 0, nullptr);
    }

    void remove(int id)
    {
        if (id < 0 || id >= static_cast<int>(nodes.size()))
            return;
        KdNode* node = &nodes[id];
        node->id = -1;
        while (node) {
            node->count -= 1;
            node = node->parent;
        }
    }

    static void recursiveFind(const QVector2D& target_pos, KdNode* node, int depth, QVector2D minp,
                              QVector2D maxp, KdNode** best_node, float* best_d2)
    {
        if (!node || node->count <= 0)
            return;

        if (node->id >= 0) {
            const float d2 = QVector2D(target_pos - node->position).lengthSquared();
            if (d2 < *best_d2) {
                *best_d2 = d2;
                *best_node = node;
            }
        }

        KdNode *first = node->left, *second = node->right;
        const bool dx = ((depth & 1) == 0);
        QVector2D split_min1, split_max1, split_min2, split_max2;
        if (dx) {
            const float split_position = node->position.x();
            const float target_v = target_pos.x();
            split_min1 = minp;
            split_max1 = QVector2D(split_position, maxp.y());
            split_min2 = QVector2D(split_position, minp.y());
            split_max2 = maxp;
            if (target_v >= split_position) {
                std::swap(first, second);
                std::swap(split_min1, split_min2);
                std::swap(split_max1, split_max2);
            }
            if (first && first->count)
                recursiveFind(target_pos, first, depth + 1, split_min1, split_max1, best_node, best_d2);
            if (second && second->count) {
                float de = target_v - split_position;
                de *= de;
                const float vy =
                    std::max(0.f, minp.y() - target_pos.y()) + std::max(0.f, target_pos.y() - maxp.y());
                de += vy * vy;
                if (de < *best_d2)
                    recursiveFind(target_pos, second, depth + 1, split_min2, split_max2, best_node, best_d2);
            }
        } else {
            const float split_position = node->position.y();
            const float target_v = target_pos.y();
            split_min1 = minp;
            split_max1 = QVector2D(maxp.x(), split_position);
            split_min2 = QVector2D(minp.x(), split_position);
            split_max2 = maxp;
            if (target_v >= split_position) {
                std::swap(first, second);
                std::swap(split_min1, split_min2);
                std::swap(split_max1, split_max2);
            }
            if (first && first->count)
                recursiveFind(target_pos, first, depth + 1, split_min1, split_max1, best_node, best_d2);
            if (second && second->count) {
                float de = target_v - split_position;
                de *= de;
                const float vx =
                    std::max(0.f, minp.x() - target_pos.x()) + std::max(0.f, target_pos.x() - maxp.x());
                de += vx * vx;
                if (de < *best_d2)
                    recursiveFind(target_pos, second, depth + 1, split_min2, split_max2, best_node, best_d2);
            }
        }
    }

    KdNode* nearest(const QVector2D& target_pos)
    {
        KdNode* best = nullptr;
        float best_d2 = std::numeric_limits<float>::infinity();
        recursiveFind(target_pos, root, 0, minp, maxp, &best, &best_d2);
        return best;
    }
};

QPainterPath orderShortest(const QPainterPath& path)
{
    auto subpaths = splitPainterPath(path);
    if (subpaths.empty())
        return path;

    std::vector<QVector2D> endpoints;
    endpoints.reserve(subpaths.size() * 2);
    for (const auto& sp : subpaths) {
        endpoints.push_back(startPoint(sp));
        endpoints.push_back(endPoint(sp));
    }

    KdTree tree(endpoints);
    std::vector<char> used(subpaths.size(), 0);
    std::vector<QPainterPath> result;
    QVector2D p(0, 0);

    for (size_t iter = 0; iter < subpaths.size(); ++iter) {
        KdNode* nb = tree.nearest(p);
        if (!nb)
            break;
        const int idb = nb->id;
        const int subpath_id = idb / 2;
        tree.remove(idb);
        tree.remove(idb ^ 1);

        if (subpath_id < 0 || subpath_id >= static_cast<int>(subpaths.size()) || used[subpath_id])
            continue;
        used[subpath_id] = 1;
        const QPainterPath& subpath = subpaths[subpath_id];
        if (idb & 1) {
            p = startPoint(subpath);
            result.push_back(subpath.toReversed());
        } else {
            p = endPoint(subpath);
            result.push_back(subpath);
        }
    }

    for (size_t i = 0; i < subpaths.size(); ++i) {
        if (!used[i])
            result.push_back(subpaths[i]);
    }

    return joinPainterPaths(result);
}

template<typename F>
QPainterPath orderByFunc(const QPainterPath& path, F&& sort_key)
{
    auto subpaths = splitPainterPath(path);
    std::sort(subpaths.begin(), subpaths.end(),
              [&](const QPainterPath& a, const QPainterPath& b) { return sort_key(a) < sort_key(b); });
    return joinPainterPaths(subpaths);
}

} // namespace

QPainterPath applyCutOrder(const QPainterPath& path, OrderStrategy strategy)
{
    switch (strategy) {
    case OrderStrategy::Normal:
        return path;
    case OrderStrategy::Reversed:
        return path.toReversed();
    case OrderStrategy::MinX:
        return orderByFunc(path, [](const QPainterPath& p) { return p.boundingRect().left(); });
    case OrderStrategy::MaxX:
        return orderByFunc(path, [](const QPainterPath& p) { return p.boundingRect().right(); });
    case OrderStrategy::MinY:
        return orderByFunc(path, [](const QPainterPath& p) { return p.boundingRect().bottom(); });
    case OrderStrategy::MaxY:
        return orderByFunc(path, [](const QPainterPath& p) { return p.boundingRect().top(); });
    case OrderStrategy::ShortestPath:
        return orderShortest(path);
    case OrderStrategy::Hilbert: {
        const QRectF b = path.boundingRect();
        const double max_size = std::max(b.width(), b.height());
        const QVector2D p0(static_cast<float>(b.left()), static_cast<float>(b.top()));
        return orderByFunc(path, [&](const QPainterPath& p) {
            return curvePosHilbert(QVector2D(startPoint(p)), p0, max_size);
        });
    }
    case OrderStrategy::ZCurve: {
        const QRectF b = path.boundingRect();
        const double max_size = std::max(b.width(), b.height());
        const QVector2D p0(static_cast<float>(b.left()), static_cast<float>(b.top()));
        return orderByFunc(path, [&](const QPainterPath& p) {
            return curvePosZ(QVector2D(startPoint(p)), p0, max_size);
        });
    }
    }
    return path;
}

} // namespace inkcut
