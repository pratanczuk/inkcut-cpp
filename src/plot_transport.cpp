// SPDX-License-Identifier: GPL-3.0-or-later

#include "plot_transport.hpp"

#include "serial_sender.hpp"

#include <QSerialPort>
#include <QTcpSocket>
#include <algorithm>

#include "i18n.hpp"

namespace inkcut {

TransportResult sendPlotPayload(const QByteArray& payload, const DeviceSetup& device)
{
    TransportResult r;

    switch (device.transport) {
    case PlotTransportKind::SerialPort: {
        QSerialPort serial;
        const SerialOpenOptions opt = serial_open_options_from_device(device);
        if (!open_serial(serial, opt)) {
            r.error_message =
                trInk("Nie można otworzyć portu %1").arg(device.port_name);
            return r;
        }
        if (!write_all(serial, payload)) {
            r.error_message = trInk("Zapis na port nie powiódł się.");
            return r;
        }
        r.bytes_written = payload.size();
        r.ok = true;
        return r;
    }
    case PlotTransportKind::TcpIp: {
        QTcpSocket sock;
        sock.connectToHost(device.tcp_host, quint16(std::max(1, device.tcp_port)));
        if (!sock.waitForConnected(5000)) {
            r.error_message = trInk("Nie można połączyć TCP %1:%2")
                                  .arg(device.tcp_host)
                                  .arg(device.tcp_port);
            return r;
        }
        qint64 total = 0;
        while (total < payload.size()) {
            const qint64 n = sock.write(payload.constData() + total, payload.size() - total);
            if (n <= 0) {
                r.error_message = trInk("Zapis TCP nie powiódł się.");
                return r;
            }
            total += n;
            if (!sock.waitForBytesWritten(5000)) {
                r.error_message = trInk("Timeout zapisu TCP.");
                return r;
            }
        }
        r.bytes_written = total;
        r.ok = true;
        return r;
    }
    }
    r.error_message = trInk("Nieobsługiwany typ transportu.");
    return r;
}

} // namespace inkcut
