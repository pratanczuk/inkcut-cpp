// SPDX-License-Identifier: GPL-3.0-or-later

#include "grbl_comm.hpp"

#include <QCoreApplication>

#include <iostream>

using namespace inkcut;

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    if (!isGrblWelcomeLine(QStringLiteral("grblHAL 1.1f ['$' or '$HELP' for help]"))) {
        std::cerr << "welcome parse failed\n";
        return 1;
    }

    int sid = -1;
    double sval = 0.0;
    if (!parseGrblSettingLine(QStringLiteral("$110=5000.000"), sid, sval) || sid != 110
        || sval < 4999.0 || sval > 5001.0) {
        std::cerr << "$$ line parse failed\n";
        return 2;
    }

    GrblStatusFrame st;
    if (!parseGrblStatusLine(QStringLiteral("<Idle|WPos:12.300,4.500,0.000|Bf:15,128>"), st)
        || !st.has_wpos || st.wpos_x != 12.3 || st.wpos_y != 4.5 || st.alarm) {
        std::cerr << "status parse failed\n";
        return 3;
    }

    if (!parseGrblStatusLine(QStringLiteral("<ALARM|WPos:0.000,0.000,0.000>"), st) || !st.alarm) {
        std::cerr << "alarm parse failed\n";
        return 4;
    }

    QString err;
    if (!isGrblErrorLine(QStringLiteral("error:24"), &err) || !err.startsWith("error")) {
        std::cerr << "error parse failed\n";
        return 5;
    }

    if (!isGrblOkLine(QStringLiteral("ok"))) {
        std::cerr << "ok parse failed\n";
        return 6;
    }

    return 0;
}
