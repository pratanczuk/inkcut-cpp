// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace inkcut {

struct GrblStatusFrame {
    QString state;
    bool has_wpos = false;
    double wpos_x = 0.0;
    double wpos_y = 0.0;
    bool has_bf = false;
    int bf_planner_free = 0;
    int bf_rx_free = 0;
    bool alarm = false;
};

bool isGrblWelcomeLine(const QString& line);
bool isGrblOkLine(const QString& line);
bool isGrblErrorLine(const QString& line, QString* error_message = nullptr);
bool parseGrblSettingLine(const QString& line, int& setting_id, double& setting_value);
bool parseGrblStatusLine(const QString& line, GrblStatusFrame& out);

} // namespace inkcut
