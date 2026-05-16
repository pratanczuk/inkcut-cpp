// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui_icons.hpp"

#include <QApplication>
#include <QStyle>
#include <QWidget>

namespace inkcut::UiIcons {

QIcon themed(QWidget* context, const char* freedesktop_name, int standard_pixmap)
{
    QIcon icon = QIcon::fromTheme(QLatin1String(freedesktop_name));
    if (!icon.isNull())
        return icon;

    QStyle* style = context ? context->style() : QApplication::style();
    return style->standardIcon(static_cast<QStyle::StandardPixmap>(standard_pixmap));
}

QIcon liveAction(LiveAction action, QWidget* context)
{
    switch (action) {
    case LiveAction::Start:
        return themed(context, "media-playback-start", QStyle::SP_MediaPlay);
    case LiveAction::Pause:
        return themed(context, "media-playback-pause", QStyle::SP_MediaPause);
    case LiveAction::Resume:
        return themed(context, "media-playback-start", QStyle::SP_MediaPlay);
    case LiveAction::Stop:
        return themed(context, "process-stop", QStyle::SP_MediaStop);
    }
    return {};
}

QIcon penUp(QWidget* context)
{
    return themed(context, "go-up", QStyle::SP_ArrowUp);
}

QIcon penDown(QWidget* context)
{
    return themed(context, "go-down", QStyle::SP_ArrowDown);
}

QIcon moveUp(QWidget* context)
{
    return themed(context, "go-up", QStyle::SP_ArrowUp);
}

QIcon moveDown(QWidget* context)
{
    return themed(context, "go-down", QStyle::SP_ArrowDown);
}

QIcon moveLeft(QWidget* context)
{
    return themed(context, "go-previous", QStyle::SP_ArrowLeft);
}

QIcon moveRight(QWidget* context)
{
    return themed(context, "go-next", QStyle::SP_ArrowRight);
}

QIcon setOrigin(QWidget* context)
{
    QIcon icon = QIcon::fromTheme(QStringLiteral("mark-location"));
    if (!icon.isNull())
        return icon;
    return themed(context, "crosshairs", QStyle::SP_FileDialogDetailedView);
}

QIcon goOrigin(QWidget* context)
{
    return themed(context, "go-home", QStyle::SP_ArrowBack);
}

QIcon systemOrigin(QWidget* context)
{
    return themed(context, "user-home", QStyle::SP_DirHomeIcon);
}

QIcon deviceConnect(bool connected, QWidget* context)
{
    if (connected)
        return themed(context, "network-offline", QStyle::SP_DialogCloseButton);
    return themed(context, "network-wired", QStyle::SP_DriveNetIcon);
}

QIcon refresh(QWidget* context)
{
    return themed(context, "view-refresh", QStyle::SP_BrowserReload);
}

QIcon clear(QWidget* context)
{
    return themed(context, "edit-clear", QStyle::SP_TrashIcon);
}

QIcon listAdd(QWidget* context)
{
    return themed(context, "list-add", QStyle::SP_FileDialogNewFolder);
}

QIcon listRemove(QWidget* context)
{
    return themed(context, "list-remove", QStyle::SP_TrashIcon);
}

QIcon materialLoad(QWidget* context)
{
    return themed(context, "go-down", QStyle::SP_ArrowDown);
}

QIcon materialUnload(QWidget* context)
{
    return themed(context, "go-up", QStyle::SP_ArrowUp);
}

} // namespace inkcut::UiIcons
