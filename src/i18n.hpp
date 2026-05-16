// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QCoreApplication>
#include <QString>

class QApplication;

namespace inkcut {

/// Wszystkie UI stringi w kontekście „Inkcut” (Qt Linguist).
inline QString trInk(const char* utf8)
{
    return QCoreApplication::translate("Inkcut", utf8);
}

void installInkcutTranslator(QApplication& app, const QString& language_code);

} // namespace inkcut
