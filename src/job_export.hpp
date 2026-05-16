// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QRectF>
#include <QString>

namespace inkcut {

struct PlotJobSettings;

/// Serializes job replay metadata (settings + bounds). Encoding UTF-8 JSON.
QString exportJobDocumentToJson(const QString& source_path, const QString& source_kind,
                                const QRectF& bounds, const PlotJobSettings& settings);

/// Best-effort import for tooling / future CLI replay.
bool importJobDocumentJson(const QByteArray& utf8_json, PlotJobSettings& settings_out,
                           QString* source_path_out, QString* source_kind_out, QRectF* bounds_out,
                           QString* error_message);

QString plotJobSettingsToJsonString(const PlotJobSettings& settings);

bool plotJobSettingsFromJsonString(const QByteArray& utf8_json, PlotJobSettings& settings_out,
                                   QString* error_message = nullptr);

} // namespace inkcut
