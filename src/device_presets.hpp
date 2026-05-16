// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "job_model.hpp"

#include <QVector>

namespace inkcut {

struct DevicePreset {
    QString id;
    QString manufacturer;
    QString model;
    double material_width = 600;
    double material_height = 400;
    PlotProtocol default_protocol = PlotProtocol::HPGL;
    int dmpl_mode = 1;
    bool swap_xy = false;
    bool mirror_x = false;
    bool mirror_y = false;
    qint32 default_baud = 9600;
};

const QVector<DevicePreset>& devicePresets();

bool devicePresetById(const QString& id, DevicePreset& out);

void applyPresetToJob(DevicePreset const& preset, PlotJobSettings& job);

} // namespace inkcut
