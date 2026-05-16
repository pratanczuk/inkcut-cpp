// SPDX-License-Identifier: GPL-3.0-or-later

#include "serial_sender.hpp"

#include "job_model.hpp"

#include <QSerialPort>

namespace {

QSerialPort::DataBits dataBitsFromInt(int bits)
{
    switch (bits) {
    case 5:
        return QSerialPort::Data5;
    case 6:
        return QSerialPort::Data6;
    case 7:
        return QSerialPort::Data7;
    default:
        return QSerialPort::Data8;
    }
}

QSerialPort::Parity parityFromInt(int parity)
{
    switch (parity) {
    case 1:
        return QSerialPort::EvenParity;
    case 2:
        return QSerialPort::OddParity;
    case 3:
        return QSerialPort::SpaceParity;
    case 4:
        return QSerialPort::MarkParity;
    default:
        return QSerialPort::NoParity;
    }
}

QSerialPort::StopBits stopBitsFromInt(int stop_bits)
{
    return stop_bits >= 2 ? QSerialPort::TwoStop : QSerialPort::OneStop;
}

QSerialPort::FlowControl flowControlFromFlags(bool rts_cts, bool dsr_dtr, bool xon_xoff)
{
    if (xon_xoff)
        return QSerialPort::SoftwareControl;
    if (rts_cts || dsr_dtr)
        return QSerialPort::HardwareControl;
    return QSerialPort::NoFlowControl;
}

} // namespace

namespace inkcut {

SerialOpenOptions serial_open_options_from_device(const DeviceSetup& device)
{
    SerialOpenOptions opt;
    opt.port_name = device.port_name;
    opt.baud_rate = device.baud_rate;
    opt.data_bits = device.data_bits;
    opt.parity = device.parity;
    opt.stop_bits = device.stop_bits;
    opt.flow_rts_cts = device.flow_rts_cts;
    opt.flow_dsr_dtr = device.flow_dsr_dtr;
    opt.flow_xon_xoff = device.flow_xon_xoff;
    return opt;
}

void configure_serial_port(QSerialPort& port, const SerialOpenOptions& opt)
{
    port.setPortName(opt.port_name);
    port.setBaudRate(opt.baud_rate, QSerialPort::AllDirections);
    port.setDataBits(dataBitsFromInt(opt.data_bits));
    port.setParity(parityFromInt(opt.parity));
    port.setStopBits(stopBitsFromInt(opt.stop_bits));
    port.setFlowControl(
        flowControlFromFlags(opt.flow_rts_cts, opt.flow_dsr_dtr, opt.flow_xon_xoff));
}

bool open_serial(QSerialPort& port, const SerialOpenOptions& opt)
{
    configure_serial_port(port, opt);
    return port.open(QIODevice::WriteOnly);
}

bool open_serial_read_write(QSerialPort& port, const SerialOpenOptions& opt)
{
    configure_serial_port(port, opt);
    return port.open(QIODevice::ReadWrite);
}

bool open_serial_read_only(QSerialPort& port, const SerialOpenOptions& opt)
{
    configure_serial_port(port, opt);
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
