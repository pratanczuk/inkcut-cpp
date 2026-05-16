// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace inkcut {

void setAppLanguage(const QString& language_code);
QString appLanguage();

/// Tekst źródłowy po polsku; dla `en` zwraca tłumaczenie z wbudowanej mapy.
QString inkcutTr(const QString& polish);

} // namespace inkcut
