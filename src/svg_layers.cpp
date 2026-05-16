// SPDX-License-Identifier: GPL-3.0-or-later

#include "svg_layers.hpp"

#include <QDomDocument>
#include <QHash>
#include <QSet>

namespace inkcut {

namespace {

QString inkscapeLabel(const QDomElement& el)
{
    return el.attribute(QStringLiteral("inkscape:label"));
}

bool isInkscapeLayer(const QDomElement& el)
{
    return el.tagName().compare(QLatin1String("g"), Qt::CaseInsensitive) == 0
        && el.attribute(QStringLiteral("inkscape:groupmode")).compare(QLatin1String("layer"),
                                                                      Qt::CaseInsensitive)
            == 0;
}

QString styleProp(const QDomElement& el, const QString& key)
{
    QString v = el.attribute(key);
    if (!v.isEmpty())
        return v.trimmed().toLower();
    const QString style = el.attribute(QStringLiteral("style"));
    for (const QString& part : style.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
        const int c = part.indexOf(QLatin1Char(':'));
        if (c > 0 && part.left(c).trimmed().compare(key, Qt::CaseInsensitive) == 0)
            return part.mid(c + 1).trimmed().toLower();
    }
    return {};
}

void collectColorsFromElement(const QDomElement& el, QSet<QString>& fills, QSet<QString>& strokes)
{
    const QString f = styleProp(el, QStringLiteral("fill"));
    if (!f.isEmpty() && f != QLatin1String("none"))
        fills.insert(f);
    const QString s = styleProp(el, QStringLiteral("stroke"));
    if (!s.isEmpty() && s != QLatin1String("none"))
        strokes.insert(s);
    for (QDomNode n = el.firstChild(); !n.isNull(); n = n.nextSibling()) {
        if (n.isElement())
            collectColorsFromElement(n.toElement(), fills, strokes);
    }
}

void walkLayers(const QDomElement& el, QVector<LayerFilterEntry>& out)
{
    if (isInkscapeLayer(el)) {
        LayerFilterEntry e;
        e.layer_id = el.attribute(QStringLiteral("id"));
        e.name = inkscapeLabel(el);
        if (e.name.isEmpty())
            e.name = e.layer_id.isEmpty() ? QStringLiteral("(warstwa)") : e.layer_id;
        const QString disp = styleProp(el, QStringLiteral("display"));
        e.enabled = disp != QLatin1String("none");
        out.push_back(e);
    }
    for (QDomNode n = el.firstChild(); !n.isNull(); n = n.nextSibling()) {
        if (n.isElement())
            walkLayers(n.toElement(), out);
    }
}

QDomElement findLayerById(QDomElement root, const QString& id)
{
    if (isInkscapeLayer(root) && root.attribute(QStringLiteral("id")) == id)
        return root;
    for (QDomNode n = root.firstChild(); !n.isNull(); n = n.nextSibling()) {
        if (!n.isElement())
            continue;
        const QDomElement found = findLayerById(n.toElement(), id);
        if (!found.isNull())
            return found;
    }
    return {};
}

bool colorDisabled(const QString& color, const QVector<ColorFilterEntry>& filters, bool fill)
{
    const QString c = color.trimmed().toLower();
    for (const ColorFilterEntry& e : filters) {
        if (e.is_fill != fill)
            continue;
        if (!e.enabled && e.color_key == c)
            return true;
    }
    return false;
}

bool elementColorDisabled(const QDomElement& el, const QVector<ColorFilterEntry>& filters)
{
    const QString f = styleProp(el, QStringLiteral("fill"));
    const QString s = styleProp(el, QStringLiteral("stroke"));
    if (!f.isEmpty() && colorDisabled(f, filters, true))
        return true;
    if (!s.isEmpty() && colorDisabled(s, filters, false))
        return true;
    return false;
}

bool nodeUsesColor(const QDomElement& el, const QString& color, bool is_fill)
{
    const QString key = is_fill ? QStringLiteral("fill") : QStringLiteral("stroke");
    const QString v = styleProp(el, key);
    const QString want = color.trimmed().toLower();
    if (!v.isEmpty() && v != QLatin1String("none") && v.trimmed().toLower() == want)
        return true;

    for (QDomNode n = el.firstChild(); !n.isNull(); n = n.nextSibling()) {
        if (n.isElement() && nodeUsesColor(n.toElement(), color, is_fill))
            return true;
    }
    return false;
}

void pruneNotColor(QDomElement& el, const QString& color, bool is_fill)
{
    QDomNode n = el.firstChild();
    while (!n.isNull()) {
        QDomNode next = n.nextSibling();
        if (n.isElement()) {
            QDomElement child = n.toElement();
            if (!nodeUsesColor(child, color, is_fill))
                el.removeChild(n);
            else
                pruneNotColor(child, color, is_fill);
        }
        n = next;
    }
}

void pruneColors(QDomElement& el, const QVector<ColorFilterEntry>& filters)
{
    QDomNode n = el.firstChild();
    while (!n.isNull()) {
        QDomNode next = n.nextSibling();
        if (n.isElement()) {
            QDomElement child = n.toElement();
            if (elementColorDisabled(child, filters))
                el.removeChild(n);
            else
                pruneColors(child, filters);
        }
        n = next;
    }
}

} // namespace

QVector<LayerFilterEntry> discoverSvgLayers(const QString& xml)
{
    QVector<LayerFilterEntry> out;
    QDomDocument doc;
    if (!doc.setContent(xml))
        return out;
    walkLayers(doc.documentElement(), out);
    return out;
}

QString filterSvgXmlOnlyLayer(const QString& xml, const QString& layer_id)
{
    if (layer_id.isEmpty())
        return xml;
    QVector<LayerFilterEntry> layers = discoverSvgLayers(xml);
    if (layers.isEmpty())
        return xml;
    for (LayerFilterEntry& e : layers)
        e.enabled = e.layer_id == layer_id;
    return filterSvgXmlByLayers(xml, layers);
}

QString filterSvgXmlByLayers(const QString& xml, const QVector<LayerFilterEntry>& layers)
{
    QDomDocument doc;
    if (!doc.setContent(xml))
        return xml;

    QDomElement root = doc.documentElement();
    for (const LayerFilterEntry& e : layers) {
        if (e.enabled || e.layer_id.isEmpty())
            continue;
        const QDomElement layer = findLayerById(root, e.layer_id);
        if (!layer.isNull() && layer.parentNode().isElement())
            layer.parentNode().toElement().removeChild(layer);
    }
    return doc.toString();
}

QVector<ColorFilterEntry> discoverSvgColors(const QString& xml)
{
    QSet<QString> fills;
    QSet<QString> strokes;
    QDomDocument doc;
    if (!doc.setContent(xml))
        return {};
    collectColorsFromElement(doc.documentElement(), fills, strokes);

    QVector<ColorFilterEntry> out;
    for (const QString& c : fills) {
        ColorFilterEntry e;
        e.color_key = c;
        e.is_fill = true;
        e.enabled = true;
        out.push_back(e);
    }
    for (const QString& c : strokes) {
        ColorFilterEntry e;
        e.color_key = c;
        e.is_fill = false;
        e.enabled = true;
        out.push_back(e);
    }
    return out;
}

QString filterSvgXmlByColors(const QString& xml, const QVector<ColorFilterEntry>& colors)
{
    if (colors.isEmpty())
        return xml;
    QDomDocument doc;
    if (!doc.setContent(xml))
        return xml;
    QDomElement root = doc.documentElement();
    pruneColors(root, colors);
    return doc.toString();
}

QString filterSvgXmlKeepOnlyColor(const QString& xml, const QString& color_key, bool is_fill)
{
    if (color_key.isEmpty())
        return xml;
    QDomDocument doc;
    if (!doc.setContent(xml))
        return xml;
    QDomElement root = doc.documentElement();
    pruneNotColor(root, color_key, is_fill);
    return doc.toString();
}

} // namespace inkcut
