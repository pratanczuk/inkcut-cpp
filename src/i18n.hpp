// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QCoreApplication>
#include <QString>

class QApplication;

namespace inkcut {

/// Wszystkie UI stringi w kontekście „Inkcut” (Qt Linguist).
/// Makro (nie funkcja inline), żeby lupdate widział translate("Inkcut", …).
#define trInk(sourceText) QCoreApplication::translate("Inkcut", sourceText)

void installInkcutTranslator(QApplication& app, const QString& language_code);

} // namespace inkcut
