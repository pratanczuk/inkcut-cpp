// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "protocols.hpp"

#include <QByteArray>
#include <optional>

namespace inkcut {

struct LivePosition {
    double x = 0;
    double y = 0;
};

/// Ostatnia pozycja z bufora RX (HPGL PU/PD lub G-code X/Y).
std::optional<LivePosition> parseLivePositionFromRx(const QByteArray& rx_accum,
                                                     PlotProtocol protocol,
                                                     double plot_scale);

} // namespace inkcut
