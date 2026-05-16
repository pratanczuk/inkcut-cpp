// SPDX-License-Identifier: GPL-3.0-or-later

#include "plot_sender.hpp"

#include "job_model.hpp"
#include "plot_live_position.hpp"

#include <QMetaObject>
#include "plot_transport.hpp"
#include "serial_sender.hpp"

#include <QRegularExpression>
#include <QSerialPort>
#include <QThread>

#include "i18n.hpp"

namespace inkcut {

PlotSendWorker::PlotSendWorker(QObject* parent) : QObject(parent) {}

void PlotSendWorker::setPayload(QByteArray payload)
{
    payload_ = std::move(payload);
}

void PlotSendWorker::setDevice(const DeviceSetup& device)
{
    if (!device_)
        device_ = new DeviceSetup();
    *device_ = device;
}

void PlotSendWorker::setLiveParse(PlotProtocol protocol, double plot_scale)
{
    live_protocol_ = protocol;
    live_plot_scale_ = plot_scale;
}

void PlotSendWorker::requestPause()
{
    paused_.store(true);
    emit pausedChanged(true);
}

void PlotSendWorker::requestResume()
{
    paused_.store(false);
    resume_requested_.store(true);
    emit pausedChanged(false);
}

void PlotSendWorker::requestCancel()
{
    cancelled_.store(true);
    paused_.store(false);
}

void PlotSendWorker::run()
{
    if (!device_ || payload_.isEmpty()) {
        emit finished(false, trInk("Brak danych lub urządzenia."));
        delete device_;
        device_ = nullptr;
        return;
    }

    if (device_->transport != PlotTransportKind::SerialPort) {
        const TransportResult tr = sendPlotPayload(payload_, *device_);
        emit progress(payload_.size(), payload_.size());
        emit finished(tr.ok, tr.error_message);
        delete device_;
        device_ = nullptr;
        return;
    }

    QSerialPort serial;
    const SerialOpenOptions opt = serial_open_options_from_device(*device_);
    if (!open_serial_read_write(serial, opt)) {
        emit finished(false,
                      trInk("Nie można otworzyć portu %1").arg(device_->port_name));
        return;
    }

    QByteArray rx_accum;
    qint64 sent = 0;
    const qint64 total = payload_.size();

    while (sent < total) {
        if (cancelled_.load()) {
            emit finished(false, QStringLiteral("Anulowano."));
            return;
        }
        while (paused_.load() && !cancelled_.load()) {
            QThread::msleep(50);
        }
        if (cancelled_.load()) {
            emit finished(false, QStringLiteral("Anulowano."));
            return;
        }

        const qint64 chunk = qMin(qint64(2048), total - sent);
        const qint64 n = serial.write(payload_.constData() + sent, chunk);
        if (n <= 0) {
            emit finished(false, trInk("Błąd zapisu na port."));
            return;
        }
        sent += n;
        emit progress(sent, total);

        if (serial.waitForBytesWritten(30000)) {
            rx_accum.append(serial.readAll());
            if (rx_accum.size() > 4096)
                rx_accum = rx_accum.right(2048);
            if (const auto pos =
                    parseLivePositionFromRx(rx_accum, live_protocol_, live_plot_scale_)) {
                emit livePosition(pos->x, pos->y);
            }
        }
        QThread::msleep(5);
    }

    serial.waitForReadyRead(100);
    emit finished(true, {});
    delete device_;
    device_ = nullptr;
}

} // namespace inkcut
