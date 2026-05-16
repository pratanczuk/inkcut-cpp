// SPDX-License-Identifier: GPL-3.0-or-later

#include "svg_path.hpp"

#include <QGuiApplication>
#include <QPainterPath>

#include <cstdio>

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);

    QPainterPath p;
    if (!inkcut::append_svg_path_d(p, QStringView(QStringLiteral("M 10 20 L 30 40 Z"))))
        return 1;

    return p.elementCount() >= 3 ? 0 : 2;
}
