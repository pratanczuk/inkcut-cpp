// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "job_model.hpp"

#include <QByteArray>
#include <QString>

namespace inkcut {

struct TransportResult {
    bool ok = false;
    QString error_message;
    qint64 bytes_written = 0;
};

TransportResult sendPlotPayload(const QByteArray& payload, const DeviceSetup& device);

} // namespace inkcut
