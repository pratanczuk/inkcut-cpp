// SPDX-License-Identifier: GPL-3.0-or-later

#include "protocols.hpp"

int main()
{
    std::string acc;
    inkcut::ProtocolSettings ps;
    ps.protocol = inkcut::PlotProtocol::GCode;
    ps.gcode.dialect = inkcut::GCodeProtocolSettings::Dialect::Grbl;
    inkcut::PlotStreamEncoder enc([&](std::string chunk) { acc += std::move(chunk); }, ps);

    enc.connection_made();
    enc.move(10.0, 12.0, 1.0, true);
    enc.finish();

    const bool has_units = acc.find("G21") != std::string::npos;
    const bool has_move = acc.find("G1 X10.000 Y12.000") != std::string::npos;
    return (has_units && has_move) ? 0 : 1;
}
