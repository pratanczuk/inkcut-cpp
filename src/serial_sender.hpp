// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QString>

class QSerialPort;

namespace inkcut {

struct SerialOpenOptions {
    QString port_name;
    qint32 baud_rate = 9600;
};

bool open_serial(QSerialPort& port, const SerialOpenOptions& opt);

/// Read-write — monitoring lub sterowanie ręczne przy jednym porcie.
bool open_serial_read_write(QSerialPort& port, const SerialOpenOptions& opt);

bool open_serial_read_only(QSerialPort& port, const SerialOpenOptions& opt);

/// Blocking write of full buffer; returns false on error.
bool write_all(QSerialPort& port, const QByteArray& data);

} // namespace inkcut
