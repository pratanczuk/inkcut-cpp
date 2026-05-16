// SPDX-License-Identifier: GPL-3.0-or-later

#include "serial_sender.hpp"

#include <QSerialPort>

namespace inkcut {

bool open_serial(QSerialPort& port, const SerialOpenOptions& opt)
{
    port.setPortName(opt.port_name);
    port.setBaudRate(opt.baud_rate, QSerialPort::AllDirections);
    port.setParity(QSerialPort::NoParity);
    port.setStopBits(QSerialPort::OneStop);
    port.setDataBits(QSerialPort::Data8);
    port.setFlowControl(QSerialPort::NoFlowControl);
    return port.open(QIODevice::WriteOnly);
}

bool open_serial_read_write(QSerialPort& port, const SerialOpenOptions& opt)
{
    port.setPortName(opt.port_name);
    port.setBaudRate(opt.baud_rate, QSerialPort::AllDirections);
    port.setParity(QSerialPort::NoParity);
    port.setStopBits(QSerialPort::OneStop);
    port.setDataBits(QSerialPort::Data8);
    port.setFlowControl(QSerialPort::NoFlowControl);
    return port.open(QIODevice::ReadWrite);
}

bool open_serial_read_only(QSerialPort& port, const SerialOpenOptions& opt)
{
    port.setPortName(opt.port_name);
    port.setBaudRate(opt.baud_rate, QSerialPort::AllDirections);
    port.setParity(QSerialPort::NoParity);
    port.setStopBits(QSerialPort::OneStop);
    port.setDataBits(QSerialPort::Data8);
    port.setFlowControl(QSerialPort::NoFlowControl);
    return port.open(QIODevice::ReadOnly);
}

bool write_all(QSerialPort& port, const QByteArray& data)
{
    qint64 total = 0;
    while (total < data.size()) {
        const qint64 n = port.write(data.constData() + total, data.size() - total);
        if (n < 0)
            return false;
        total += n;
        if (!port.waitForBytesWritten(5000))
            return false;
    }
    return true;
}

} // namespace inkcut
