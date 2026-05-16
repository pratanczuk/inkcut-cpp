// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace inkcut {

/// Konwersja wartości zapisanych wewnętrznie w mm ↔ jednostki wyświetlania.
double mmToDisplay(double mm, const QString& units);
double displayToMm(double display, const QString& units);
QString unitSuffix(const QString& units);
QString unitLabel(const QString& units);

} // namespace inkcut
