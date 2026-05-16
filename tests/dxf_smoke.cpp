// SPDX-License-Identifier: GPL-3.0-or-later

#include "dxf_document.hpp"

#include <QGuiApplication>
#include <QPainterPath>
#include <cstdio>

namespace {

const char* kMinimalLine = R"DXF(
0
SECTION
2
ENTITIES
0
LINE
10
0
20
0
11
50
21
30
0
ENDSEC
)DXF";

} // namespace

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);

    QPainterPath p;
    QString err;
    if (!inkcut::loadDxfPainterPath(QString::fromLatin1(kMinimalLine), p, &err)) {
        std::fprintf(stderr, "%s\n", qPrintable(err));
        return 1;
    }
    if (p.isEmpty()) {
        std::fprintf(stderr, "empty path\n");
        return 1;
    }
    return 0;
}
