// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "job_model.hpp"

#include <QString>
#include <QStringList>

namespace inkcut {

/// Discover Inkscape layers from SVG XML.
QVector<LayerFilterEntry> discoverSvgLayers(const QString& xml);

/// Remove disabled layers from XML before path load.
QString filterSvgXmlByLayers(const QString& xml, const QVector<LayerFilterEntry>& layers);

/// Zostaw tylko jedną warstwę Inkscape (do osobnego cięcia / powtórzeń).
QString filterSvgXmlOnlyLayer(const QString& xml, const QString& layer_id);

/// Discover unique fill/stroke colors in SVG (hex keys).
QVector<ColorFilterEntry> discoverSvgColors(const QString& xml);

/// Strip elements whose fill/stroke matches a disabled color filter.
QString filterSvgXmlByColors(const QString& xml, const QVector<ColorFilterEntry>& colors);

/// Zostaw tylko elementy używające danego koloru wypełnienia lub obrysu.
QString filterSvgXmlKeepOnlyColor(const QString& xml, const QString& color_key, bool is_fill);

} // namespace inkcut
