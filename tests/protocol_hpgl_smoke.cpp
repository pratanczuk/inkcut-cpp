// SPDX-License-Identifier: GPL-3.0-or-later

#include "protocols.hpp"

#include <cstring>

int main()
{
    std::string acc;
    inkcut::ProtocolSettings ps;
    inkcut::PlotStreamEncoder enc([&](std::string chunk) { acc += std::move(chunk); }, ps);

    enc.connection_made();
    enc.finish();

    return acc.find("IN") != std::string::npos ? 0 : 1;
}
