// SPDX-License-Identifier: GPL-3.0-or-later

#include "path_utils.hpp"

#include <QLineF>
#include <cmath>
namespace inkcut {

namespace {

void finishCurve(QPainterPath& p, std::vector<QPointF>& params)
{
    if (params.size() == 2)
        p.quadTo(params[0], params[1]);
    else if (params.size() == 3)
        p.cubicTo(params[0], params[1], params[2]);
    params.clear();
}

} // namespace

std::vector<QPainterPath> splitPainterPath(const QPainterPath& path)
{
    std::vector<QPainterPath> subpaths;
    std::vector<QPointF> params;
    QPainterPath* current = nullptr;

    const int n = path.elementCount();
    for (int i = 0; i < n; ++i) {
        const QPainterPath::Element e = path.elementAt(i);

        if (!params.empty() && e.type != QPainterPath::CurveToDataElement)
            finishCurve(*current, params);

        switch (e.type) {
        case QPainterPath::MoveToElement:
            subpaths.emplace_back();
            current = &subpaths.back();
            current->moveTo(e.x, e.y);
            break;
        case QPainterPath::LineToElement:
            if (!current)
                break;
            current->lineTo(e.x, e.y);
            break;
        case QPainterPath::CurveToElement:
            if (!current)
                break;
            params = { QPointF(e.x, e.y) };
            break;
        case QPainterPath::CurveToDataElement:
            params.emplace_back(e.x, e.y);
            break;
        default:
            break;
        }
    }
    if (!params.empty() && !subpaths.empty())
        finishCurve(subpaths.back(), params);

    return subpaths;
}

QPainterPath joinPainterPaths(const std::vector<QPainterPath>& paths)
{
    QPainterPath result;
    for (const QPainterPath& p : paths)
        result.addPath(p);
    return result;
}

QPointF pathElementToPoint(const QPainterPath::Element& e)
{
    return QPointF(e.x, e.y);
}

void addItemToPath(QPainterPath& result, const QPainterPath::Element& e, int i,
                   const std::vector<QPainterPath::Element>& items)
{
    const int element_count = static_cast<int>(items.size());
    const QPointF position = pathElementToPoint(e);

    if (e.type == QPainterPath::MoveToElement)
        result.moveTo(position);
    else if (e.type == QPainterPath::LineToElement)
        result.lineTo(position);
    else if (e.type == QPainterPath::CurveToDataElement)
        return;
    else if (e.type == QPainterPath::CurveToElement) {
        std::vector<QPointF> params = { position };
        int j = i + 1;
        while (j < element_count) {
            const auto& next_item = items[j];
            if (next_item.type != QPainterPath::CurveToDataElement)
                break;
            params.emplace_back(pathElementToPoint(next_item));
            ++j;
        }
        if (params.size() == 2)
            result.quadTo(params[0], params[1]);
        else if (params.size() == 3)
            result.cubicTo(params[0], params[1], params[2]);
    }
}

std::vector<QPainterPath::Element> pathToElements(const QPainterPath& path)
{
    std::vector<QPainterPath::Element> out;
    const int n = path.elementCount();
    out.reserve(n);
    for (int i = 0; i < n; ++i)
        out.push_back(path.elementAt(i));
    return out;
}

QPainterPath pathFromElements(const std::vector<QPainterPath::Element>& elements)
{
    QPainterPath result;
    const int n = static_cast<int>(elements.size());
    for (int i = 0; i < n; ++i)
        addItemToPath(result, elements[i], i, elements);
    return result;
}

double trailingAngle(const QPainterPath& path)
{
    const int count = path.elementCount();
    if (count < 10)
        return path.angleAtPercent(1.0);

    const auto els = pathToElements(path);
    QPainterPath tail;
    for (int pos = count - 5; pos < count; ++pos)
        addItemToPath(tail, els[pos], pos, els);

    return tail.angleAtPercent(1.0);
}

} // namespace inkcut
