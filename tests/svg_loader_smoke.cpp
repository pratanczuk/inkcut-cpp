// SPDX-License-Identifier: GPL-3.0-or-later

#include "svg_document.hpp"

#include <QGuiApplication>
#include <QPainterPath>
#include <cstdio>

namespace {

bool load_ok(const char* xml, QPainterPath& out, QString* err)
{
    return inkcut::loadSvgPainterPath(QString::fromUtf8(xml), out, err);
}

bool nonempty_path(const QPainterPath& p)
{
    return !p.isEmpty();
}

} // namespace

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);

    int failures = 0;

    auto check = [&](bool cond, const char* msg) {
        if (!cond) {
            std::fprintf(stderr, "%s\n", msg);
            ++failures;
        }
    };

    {
        QPainterPath p;
        QString err;
        check(load_ok("<svg xmlns=\"http://www.w3.org/2000/svg\"><rect width=\"10\" height=\"10\"/></svg>", p,
                      &err)
                  && err.isEmpty()
                  && nonempty_path(p),
              "rect outline expected");
    }

    {
        QPainterPath p;
        QString err;
        const char* svg = R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100">
  <defs><clipPath id="c" clipPathUnits="objectBoundingBox"><rect x="0.25" y="0.25" width="0.5" height="0.5"/></clipPath></defs>
  <rect x="0" y="0" width="100" height="100" clip-path="url(#c)"/>
</svg>)SVG";
        check(load_ok(svg, p, &err) && err.isEmpty() && nonempty_path(p), "obb clip expected");
    }

    {
        QPainterPath p;
        QString err;
        const char* svg =
            R"(<svg xmlns="http://www.w3.org/2000/svg"><text x="5" y="20" font-size="12">Hi</text></svg>)";
        check(load_ok(svg, p, &err) && err.isEmpty() && nonempty_path(p), "text outline expected");
    }

    {
        QPainterPath p;
        QString err;
        const char* svg = R"(<svg xmlns="http://www.w3.org/2000/svg"><text x="0" y="16" font-size="12">
  <tspan x="40">XY</tspan></text></svg>)";
        check(load_ok(svg, p, &err) && err.isEmpty() && nonempty_path(p), "tspan x offset expected");
    }

    {
        QPainterPath p;
        QString err;
        const char* svg = R"(<svg xmlns="http://www.w3.org/2000/svg"><text x="10 40" y="20" font-size="12">AB</text></svg>)";
        check(load_ok(svg, p, &err) && err.isEmpty() && nonempty_path(p),
              "multi-coordinate text x expected");
    }

    {
        QPainterPath p;
        QString err;
        const char* svg =
            R"(<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink">
  <defs><path id="curve" d="M 0 50 Q 50 0 100 50"/></defs>
  <text font-size="14"><textPath xlink:href="#curve">Hi</textPath></text>
</svg>)";
        check(load_ok(svg, p, &err) && err.isEmpty() && nonempty_path(p), "textPath outline expected");
    }

    {
        QPainterPath p;
        QString err;
        const char* svg =
            R"(<svg xmlns="http://www.w3.org/2000/svg"><text x="50" y="20" font-size="12" text-anchor="end">Hi</text></svg>)";
        check(load_ok(svg, p, &err) && err.isEmpty() && nonempty_path(p),
              "text-anchor end expected");
    }

    return failures > 0 ? 1 : 0;
}
