// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QIcon>

class QWidget;

namespace inkcut::UiIcons {

enum class LiveAction { Start, Pause, Resume, Stop };

QIcon themed(QWidget* context, const char* freedesktop_name, int standard_pixmap);

QIcon liveAction(LiveAction action, QWidget* context = nullptr);
QIcon penUp(QWidget* context = nullptr);
QIcon penDown(QWidget* context = nullptr);
QIcon moveUp(QWidget* context = nullptr);
QIcon moveDown(QWidget* context = nullptr);
QIcon moveLeft(QWidget* context = nullptr);
QIcon moveRight(QWidget* context = nullptr);
QIcon setOrigin(QWidget* context = nullptr);
QIcon goOrigin(QWidget* context = nullptr);
QIcon systemOrigin(QWidget* context = nullptr);
QIcon deviceConnect(bool connected, QWidget* context = nullptr);
QIcon refresh(QWidget* context = nullptr);
QIcon clear(QWidget* context = nullptr);
QIcon listAdd(QWidget* context = nullptr);
QIcon listRemove(QWidget* context = nullptr);
QIcon materialLoad(QWidget* context = nullptr);
QIcon materialUnload(QWidget* context = nullptr);

} // namespace inkcut::UiIcons
