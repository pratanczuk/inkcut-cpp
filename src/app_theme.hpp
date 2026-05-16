// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

class QApplication;

namespace inkcut {

void applyApplicationTheme(QApplication& app, const QString& dock_style);

} // namespace inkcut
