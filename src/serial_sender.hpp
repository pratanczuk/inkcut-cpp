// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QString>

class QSerialPort;

namespace inkcut {

struct DeviceSetup;

struct SerialOpenOptions {
    QString port_name;
    qint32 baud_rate = 115200;
    int data_bits = 8;
    int parity = 0;
    int stop_bits = 1;
    bool flow_rts_cts = false;
    bool flow_dsr_dtr = false;
    bool flow_xon_xoff = false;
};

/// Ustawia parametry linii (bez otwierania portu).
void configure_serial_port(QSerialPort& port, const SerialOpenOptions& opt);

SerialOpenOptions serial_open_options_from_device(const DeviceSetup& device);

bool open_serial(QSerialPort& port, const SerialOpenOptions& opt);

/// Read-write — monitoring lub sterowanie ręczne przy jednym porcie.
bool open_serial_read_write(QSerialPort& port, const SerialOpenOptions& opt);

bool open_serial_read_only(QSerialPort& port, const SerialOpenOptions& opt);

/// Blocking write of full buffer; returns false on error.
bool write_all(QSerialPort& port, const QByteArray& data);

} // namespace inkcut
