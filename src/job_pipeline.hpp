// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "job_model.hpp"

#include <QPainterPath>
#include <QStringList>
#include <string>

namespace inkcut {

class DevicePlugin;

/// SVG z uwzględnieniem filtrów warstw/kolorów i powtórzeń na warstwę.
bool loadSvgDesignPath(const QString& xml, const QString& document_base_dir, const PlotJobSettings& settings,
                       QPainterPath& out, QString* error_message, QStringList* warnings = nullptr);

int maxEnabledLayerPassCount(const QVector<LayerFilterEntry>& layers);
int maxEnabledColorPassCount(const QVector<ColorFilterEntry>& colors);

QPainterPath processJobPath(const QPainterPath& raw_path, const PlotJobSettings& settings,
                            DevicePlugin* device_plugin = nullptr);

/// Wypełnione sylwetki z potrace — bez kolejności cięcia, min-line i offsetu ostrza.
QPainterPath processFilledTracePath(const QPainterPath& raw_path, const PlotJobSettings& settings,
                                    DevicePlugin* device_plugin = nullptr);

std::string buildPlotProgram(const QPainterPath& raw_path, const PlotJobSettings& settings,
                             DevicePlugin* device_plugin = nullptr,
                             bool filled_silhouette = false);

PlotProtocol plotProtocolFromCli(QStringView name);

QString plotProtocolToCli(PlotProtocol p);

QString orderStrategyToCli(OrderStrategy o);

bool orderStrategyFromCli(QStringView name, OrderStrategy& out);

} // namespace inkcut
