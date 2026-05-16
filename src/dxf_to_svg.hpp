// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QString>

namespace inkcut {

/// Konwersja DXF (ASCII lub binarny) do dokumentu SVG (ścieżki w jednostkach DXF).
bool convertDxfBytesToSvg(const QByteArray& dxf_raw, QString& svg_xml_out,
                          QString* error_message = nullptr);

bool isDxfToSvgAvailable();

} // namespace inkcut
