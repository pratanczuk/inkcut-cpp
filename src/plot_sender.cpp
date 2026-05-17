// SPDX-License-Identifier: GPL-3.0-or-later

#include "plot_sender.hpp"

#include "grbl_comm.hpp"
#include "job_model.hpp"
#include "plot_live_position.hpp"

#include <QMetaObject>
#include "plot_transport.hpp"
#include "serial_sender.hpp"

#include <QElapsedTimer>
#include <QRegularExpression>
#include <QSerialPort>
#include <QTcpSocket>
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

void PlotSendWorker::setProtocolSettings(const ProtocolSettings& protocol)
{
    protocol_ = protocol;
}

namespace {

QByteArray normalizeCommands(const QString& commands)
{
    if (commands.trimmed().isEmpty())
        return {};
    QString payload = commands;
    payload.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
    payload.replace(QStringLiteral("\\r"), QStringLiteral("\r"));
    return payload.toUtf8();
}

bool writeCommand(QSerialPort& serial, const QByteArray& bytes)
{
    if (bytes.isEmpty())
        return true;
    return write_all(serial, bytes);
}

bool waitForLine(QSerialPort& serial, QByteArray& rx_buffer, QString& out_line, int timeout_ms)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeout_ms) {
        const int nl = rx_buffer.indexOf('\n');
        if (nl >= 0) {
            QByteArray line = rx_buffer.left(nl);
            rx_buffer.remove(0, nl + 1);
            line = line.trimmed();
            if (!line.isEmpty()) {
                out_line = QString::fromLatin1(line);
                return true;
            }
        }
        if (!serial.waitForReadyRead(50))
            continue;
        rx_buffer.append(serial.readAll());
        if (rx_buffer.size() > 16384)
            rx_buffer = rx_buffer.right(8192);
    }
    return false;
}

bool waitForAckOrError(QSerialPort& serial, QByteArray& rx_buffer, int timeout_ms, QString& err)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeout_ms) {
        QString line;
        if (!waitForLine(serial, rx_buffer, line, 100))
            continue;
        if (isGrblOkLine(line))
            return true;
        if (isGrblErrorLine(line, &err))
            return false;
    }
    err = QObject::tr("Timeout waiting for GRBL ack.");
    return false;
}

bool sendAndWaitAck(QSerialPort& serial, QByteArray& rx_buffer, const QByteArray& bytes, QString& err)
{
    if (!writeCommand(serial, bytes)) {
        err = QObject::tr("Serial write failed.");
        return false;
    }
    return waitForAckOrError(serial, rx_buffer, 4000, err);
}

bool sendCommandAndOptionalAck(QSerialPort& serial, QByteArray& rx_buffer, const QByteArray& bytes,
                               QString& err)
{
    if (bytes.isEmpty())
        return true;
    if (!writeCommand(serial, bytes)) {
        err = QObject::tr("Serial write failed.");
        return false;
    }
    // Multi-line custom blocks may be HPGL-like and not emit "ok".
    if (bytes.trimmed().contains('\n'))
        return true;
    return waitForAckOrError(serial, rx_buffer, 4000, err);
}

bool writeCommand(QTcpSocket& sock, const QByteArray& bytes)
{
    if (bytes.isEmpty())
        return true;
    qint64 total = 0;
    while (total < bytes.size()) {
        const qint64 n = sock.write(bytes.constData() + total, bytes.size() - total);
        if (n <= 0)
            return false;
        total += n;
        if (!sock.waitForBytesWritten(5000))
            return false;
    }
    return true;
}

bool waitForLine(QTcpSocket& sock, QByteArray& rx_buffer, QString& out_line, int timeout_ms)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeout_ms) {
        const int nl = rx_buffer.indexOf('\n');
        if (nl >= 0) {
            QByteArray line = rx_buffer.left(nl);
            rx_buffer.remove(0, nl + 1);
            line = line.trimmed();
            if (!line.isEmpty()) {
                out_line = QString::fromLatin1(line);
                return true;
            }
        }
        if (!sock.waitForReadyRead(50))
            continue;
        rx_buffer.append(sock.readAll());
        if (rx_buffer.size() > 16384)
            rx_buffer = rx_buffer.right(8192);
    }
    return false;
}

bool waitForAckOrError(QTcpSocket& sock, QByteArray& rx_buffer, int timeout_ms, QString& err)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeout_ms) {
        QString line;
        if (!waitForLine(sock, rx_buffer, line, 100))
            continue;
        if (isGrblOkLine(line))
            return true;
        if (isGrblErrorLine(line, &err))
            return false;
    }
    err = QObject::tr("Timeout waiting for GRBL ack.");
    return false;
}

bool sendAndWaitAck(QTcpSocket& sock, QByteArray& rx_buffer, const QByteArray& bytes, QString& err)
{
    if (!writeCommand(sock, bytes)) {
        err = QObject::tr("TCP write failed.");
        return false;
    }
    return waitForAckOrError(sock, rx_buffer, 4000, err);
}

bool sendCommandAndOptionalAck(QTcpSocket& sock, QByteArray& rx_buffer, const QByteArray& bytes,
                               QString& err)
{
    if (bytes.isEmpty())
        return true;
    if (!writeCommand(sock, bytes)) {
        err = QObject::tr("TCP write failed.");
        return false;
    }
    if (bytes.trimmed().contains('\n'))
        return true;
    return waitForAckOrError(sock, rx_buffer, 4000, err);
}

QList<QByteArray> splitProgramLines(const QByteArray& payload)
{
    QList<QByteArray> out;
    const QList<QByteArray> raw = payload.split('\n');
    out.reserve(raw.size());
    for (QByteArray line : raw) {
        line = line.trimmed();
        if (!line.isEmpty())
            out.push_back(line);
    }
    return out;
}

bool grblHandshake(QSerialPort& serial, QByteArray& rx_buffer, QString& err)
{
    // Wake/reset: Ctrl+X + newline.
    QByteArray wake;
    wake.append(char(0x18));
    wake.append('\n');
    if (!write_all(serial, wake)) {
        err = QObject::tr("Cannot send wake/reset to controller.");
        return false;
    }

    // Wait for welcome.
    bool saw_welcome = false;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 5000) {
        QString line;
        if (!waitForLine(serial, rx_buffer, line, 200))
            continue;
        if (isGrblWelcomeLine(line)) {
            saw_welcome = true;
            break;
        }
    }
    if (!saw_welcome) {
        err = QObject::tr("No GRBL/grblHAL welcome after reset.");
        return false;
    }
    return true;
}

bool grblHandshake(QTcpSocket& sock, QByteArray& rx_buffer, QString& err)
{
    QByteArray wake;
    wake.append(char(0x18));
    wake.append('\n');
    if (!writeCommand(sock, wake)) {
        err = QObject::tr("Cannot send wake/reset to controller.");
        return false;
    }
    bool saw_welcome = false;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 5000) {
        QString line;
        if (!waitForLine(sock, rx_buffer, line, 200))
            continue;
        if (isGrblWelcomeLine(line)) {
            saw_welcome = true;
            break;
        }
    }
    if (!saw_welcome) {
        err = QObject::tr("No GRBL/grblHAL welcome after reset.");
        return false;
    }
    return true;
}

} // namespace

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

    QByteArray rx_accum;
    auto finish_and_cleanup = [this]() {
        delete device_;
        device_ = nullptr;
    };
    const bool gcode = live_protocol_ == PlotProtocol::GCode;
    if (gcode && device_->transport == PlotTransportKind::TcpIp) {
        QTcpSocket sock;
        sock.connectToHost(device_->tcp_host, quint16(std::max(1, device_->tcp_port)));
        if (!sock.waitForConnected(5000)) {
            emit finished(false, trInk("Nie można połączyć TCP %1:%2")
                                     .arg(device_->tcp_host)
                                     .arg(device_->tcp_port));
            finish_and_cleanup();
            return;
        }
        QString err;
        if (!grblHandshake(sock, rx_accum, err)) {
            emit finished(false, err);
            finish_and_cleanup();
            return;
        }
        if (!sendCommandAndOptionalAck(sock, rx_accum,
                                       normalizeCommands(device_->before_connect_command), err)) {
            emit finished(false, err);
            finish_and_cleanup();
            return;
        }
        if (!sendCommandAndOptionalAck(sock, rx_accum,
                                       normalizeCommands(device_->after_connect_command), err)) {
            emit finished(false, err);
            finish_and_cleanup();
            return;
        }
        if (!writeCommand(sock, QByteArray("?\n"))) {
            emit finished(false, trInk("Błąd zapisu na TCP."));
            finish_and_cleanup();
            return;
        }
        {
            QElapsedTimer timer;
            timer.start();
            bool got_status = false;
            while (timer.elapsed() < 2000) {
                QString line;
                if (!waitForLine(sock, rx_accum, line, 100))
                    continue;
                GrblStatusFrame st;
                if (!parseGrblStatusLine(line, st))
                    continue;
                got_status = true;
                if (st.alarm) {
                    emit finished(false, trInk("GRBL jest w stanie ALARM."));
                    finish_and_cleanup();
                    return;
                }
                if (st.has_wpos)
                    emit livePosition(st.wpos_x, st.wpos_y);
                break;
            }
            if (!got_status) {
                emit finished(false, trInk("Brak odpowiedzi statusu z GRBL."));
                finish_and_cleanup();
                return;
            }
        }
        grbl_settings_cache_.clear();
        if (!writeCommand(sock, QByteArray("$$\n"))) {
            emit finished(false, trInk("Błąd zapisu na TCP."));
            finish_and_cleanup();
            return;
        }
        {
            QElapsedTimer timer;
            timer.start();
            while (timer.elapsed() < 5000) {
                QString line;
                if (!waitForLine(sock, rx_accum, line, 100))
                    continue;
                int sid = -1;
                double sval = 0.0;
                if (parseGrblSettingLine(line, sid, sval)) {
                    grbl_settings_cache_.insert(sid, sval);
                    continue;
                }
                if (isGrblOkLine(line))
                    break;
                QString line_err;
                if (isGrblErrorLine(line, &line_err)) {
                    emit finished(false, line_err);
                    finish_and_cleanup();
                    return;
                }
            }
        }
        emit grblSettingsReady(grbl_settings_cache_);
        const bool restore_10 = grbl_settings_cache_.contains(10);
        const bool restore_32 = grbl_settings_cache_.contains(32);
        if (!sendAndWaitAck(sock, rx_accum, QByteArray("$10=1\n"), err)) {
            emit finished(false, err);
            finish_and_cleanup();
            return;
        }
        if (protocol_.gcode.lift_mode == GCodeProtocolSettings::SolenoidPwm) {
            if (!sendAndWaitAck(sock, rx_accum, QByteArray("$32=0\n"), err)) {
                emit finished(false, err);
                finish_and_cleanup();
                return;
            }
        }
        if (!sendCommandAndOptionalAck(sock, rx_accum,
                                       normalizeCommands(device_->before_job_command), err)) {
            emit finished(false, err);
            finish_and_cleanup();
            return;
        }
        QList<QByteArray> lines = splitProgramLines(payload_);
        QList<int> in_flight_lengths;
        int in_flight_bytes = 0;
        const int max_rx_bytes = 127;
        int next_line = 0;
        qint64 sent = 0;
        const qint64 total = payload_.size();
        QElapsedTimer poll_timer;
        poll_timer.start();
        QElapsedTimer watchdog;
        watchdog.start();
        while (next_line < lines.size() || !in_flight_lengths.isEmpty()) {
            if (cancelled_.load()) {
                emit finished(false, QStringLiteral("Anulowano."));
                finish_and_cleanup();
                return;
            }
            while (paused_.load() && !cancelled_.load()) {
                QThread::msleep(50);
            }
            if (cancelled_.load()) {
                emit finished(false, QStringLiteral("Anulowano."));
                finish_and_cleanup();
                return;
            }
            while (next_line < lines.size()) {
                const QByteArray line_to_send = lines[next_line] + QByteArray("\n");
                const int len = line_to_send.size();
                if (len > max_rx_bytes) {
                    emit finished(false, trInk("Linia G-code jest dłuższa niż bufor GRBL."));
                    finish_and_cleanup();
                    return;
                }
                if (in_flight_bytes + len > max_rx_bytes)
                    break;
                if (!writeCommand(sock, line_to_send)) {
                    emit finished(false, trInk("Błąd zapisu na TCP."));
                    finish_and_cleanup();
                    return;
                }
                in_flight_lengths.push_back(len);
                in_flight_bytes += len;
                ++next_line;
            }
            QString line;
            if (waitForLine(sock, rx_accum, line, 120)) {
                watchdog.restart();
                if (isGrblOkLine(line)) {
                    if (!in_flight_lengths.isEmpty()) {
                        sent += in_flight_lengths.front();
                        in_flight_bytes -= in_flight_lengths.front();
                        in_flight_lengths.pop_front();
                        emit progress(qMin(sent, total), total);
                    }
                } else if (isGrblErrorLine(line, &err)) {
                    emit finished(false, err);
                    finish_and_cleanup();
                    return;
                } else {
                    GrblStatusFrame st;
                    if (parseGrblStatusLine(line, st) && st.has_wpos)
                        emit livePosition(st.wpos_x, st.wpos_y);
                }
            } else if (watchdog.elapsed() > 5000) {
                emit finished(false, trInk("Timeout odpowiedzi GRBL podczas streamingu."));
                finish_and_cleanup();
                return;
            }
            if (poll_timer.elapsed() >= 150) {
                poll_timer.restart();
                if (writeCommand(sock, QByteArray("?\n"))) {
                    QString status_line;
                    if (waitForLine(sock, rx_accum, status_line, 60)) {
                        GrblStatusFrame st;
                        if (parseGrblStatusLine(status_line, st) && st.has_wpos)
                            emit livePosition(st.wpos_x, st.wpos_y);
                    }
                }
            }
        }
        if (!sendCommandAndOptionalAck(sock, rx_accum,
                                       normalizeCommands(device_->after_job_command), err)) {
            emit finished(false, err);
            finish_and_cleanup();
            return;
        }
        if (restore_10) {
            const QByteArray cmd = QByteArray("$10=")
                                       + QByteArray::number(int(grbl_settings_cache_.value(10)))
                                       + QByteArray("\n");
            (void)sendAndWaitAck(sock, rx_accum, cmd, err);
        }
        if (restore_32) {
            const QByteArray cmd = QByteArray("$32=")
                                       + QByteArray::number(int(grbl_settings_cache_.value(32)))
                                       + QByteArray("\n");
            (void)sendAndWaitAck(sock, rx_accum, cmd, err);
        }
        emit grblSettingsReady(grbl_settings_cache_);
        emit finished(true, {});
        finish_and_cleanup();
        return;
    }

    QSerialPort serial;
    const SerialOpenOptions opt = serial_open_options_from_device(*device_);
    if (!open_serial_read_write(serial, opt)) {
        emit finished(false,
                      trInk("Nie można otworzyć portu %1").arg(device_->port_name));
        return;
    }

    if (gcode) {
        QString err;
        if (!grblHandshake(serial, rx_accum, err)) {
            emit finished(false, err);
            finish_and_cleanup();
            return;
        }

        // Pre-connect custom commands.
        if (!sendCommandAndOptionalAck(serial, rx_accum,
                                       normalizeCommands(device_->before_connect_command), err)) {
            emit finished(false, err);
            finish_and_cleanup();
            return;
        }
        if (!sendCommandAndOptionalAck(serial, rx_accum,
                                       normalizeCommands(device_->after_connect_command), err)) {
            emit finished(false, err);
            finish_and_cleanup();
            return;
        }

        // Probe status and ensure not ALARM.
        if (!write_all(serial, QByteArray("?\n"))) {
            emit finished(false, trInk("Błąd zapisu na port."));
            finish_and_cleanup();
            return;
        }
        {
            QElapsedTimer timer;
            timer.start();
            bool got_status = false;
            while (timer.elapsed() < 2000) {
                QString line;
                if (!waitForLine(serial, rx_accum, line, 100))
                    continue;
                GrblStatusFrame st;
                if (!parseGrblStatusLine(line, st))
                    continue;
                got_status = true;
                if (st.alarm) {
                    emit finished(false, trInk("GRBL jest w stanie ALARM."));
                    finish_and_cleanup();
                    return;
                }
                if (st.has_wpos)
                    emit livePosition(st.wpos_x, st.wpos_y);
                break;
            }
            if (!got_status) {
                emit finished(false, trInk("Brak odpowiedzi statusu z GRBL."));
                finish_and_cleanup();
                return;
            }
        }

        // Read $$ cache.
        grbl_settings_cache_.clear();
        if (!write_all(serial, QByteArray("$$\n"))) {
            emit finished(false, trInk("Błąd zapisu na port."));
            finish_and_cleanup();
            return;
        }
        {
            QElapsedTimer timer;
            timer.start();
            while (timer.elapsed() < 5000) {
                QString line;
                if (!waitForLine(serial, rx_accum, line, 100))
                    continue;
                int sid = -1;
                double sval = 0.0;
                if (parseGrblSettingLine(line, sid, sval)) {
                    grbl_settings_cache_.insert(sid, sval);
                    continue;
                }
                if (isGrblOkLine(line))
                    break;
                QString line_err;
                if (isGrblErrorLine(line, &line_err)) {
                    emit finished(false, line_err);
                    finish_and_cleanup();
                    return;
                }
            }
        }
        emit grblSettingsReady(grbl_settings_cache_);

        // Dynamic overrides (save+set, restore in tail).
        const bool restore_10 = grbl_settings_cache_.contains(10);
        const bool restore_32 = grbl_settings_cache_.contains(32);
        if (!sendAndWaitAck(serial, rx_accum, QByteArray("$10=1\n"), err)) {
            emit finished(false, err);
            finish_and_cleanup();
            return;
        }
        if (protocol_.gcode.lift_mode == GCodeProtocolSettings::SolenoidPwm) {
            if (!sendAndWaitAck(serial, rx_accum, QByteArray("$32=0\n"), err)) {
                emit finished(false, err);
                finish_and_cleanup();
                return;
            }
        }

        if (!sendCommandAndOptionalAck(serial, rx_accum,
                                       normalizeCommands(device_->before_job_command), err)) {
            emit finished(false, err);
            finish_and_cleanup();
            return;
        }

        // Character-counting streaming (GRBL RX buffer aware).
        QList<QByteArray> lines = splitProgramLines(payload_);
        QList<int> in_flight_lengths;
        int in_flight_bytes = 0;
        const int max_rx_bytes = 127;
        int next_line = 0;
        qint64 sent = 0;
        const qint64 total = payload_.size();
        QElapsedTimer poll_timer;
        poll_timer.start();
        QElapsedTimer watchdog;
        watchdog.start();

        while (next_line < lines.size() || !in_flight_lengths.isEmpty()) {
            if (cancelled_.load()) {
                emit finished(false, QStringLiteral("Anulowano."));
                finish_and_cleanup();
                return;
            }
            while (paused_.load() && !cancelled_.load()) {
                QThread::msleep(50);
            }
            if (cancelled_.load()) {
                emit finished(false, QStringLiteral("Anulowano."));
                finish_and_cleanup();
                return;
            }

            // Fill controller RX window.
            while (next_line < lines.size()) {
                const QByteArray line_to_send = lines[next_line] + QByteArray("\n");
                const int len = line_to_send.size();
                if (len > max_rx_bytes) {
                    emit finished(false, trInk("Linia G-code jest dłuższa niż bufor GRBL."));
                    finish_and_cleanup();
                    return;
                }
                if (in_flight_bytes + len > max_rx_bytes)
                    break;
                if (!write_all(serial, line_to_send)) {
                    emit finished(false, trInk("Błąd zapisu na port."));
                    finish_and_cleanup();
                    return;
                }
                in_flight_lengths.push_back(len);
                in_flight_bytes += len;
                ++next_line;
            }

            QString line;
            if (waitForLine(serial, rx_accum, line, 120)) {
                watchdog.restart();
                if (isGrblOkLine(line)) {
                    if (!in_flight_lengths.isEmpty()) {
                        sent += in_flight_lengths.front();
                        in_flight_bytes -= in_flight_lengths.front();
                        in_flight_lengths.pop_front();
                        emit progress(qMin(sent, total), total);
                    }
                } else if (isGrblErrorLine(line, &err)) {
                    emit finished(false, err);
                    finish_and_cleanup();
                    return;
                } else {
                    GrblStatusFrame st;
                    if (parseGrblStatusLine(line, st) && st.has_wpos)
                        emit livePosition(st.wpos_x, st.wpos_y);
                }
            } else if (watchdog.elapsed() > 5000) {
                emit finished(false, trInk("Timeout odpowiedzi GRBL podczas streamingu."));
                finish_and_cleanup();
                return;
            }

            // Background status poll every ~150 ms.
            if (poll_timer.elapsed() >= 150) {
                poll_timer.restart();
                if (write_all(serial, QByteArray("?\n"))) {
                    QString status_line;
                    if (waitForLine(serial, rx_accum, status_line, 60)) {
                        GrblStatusFrame st;
                        if (parseGrblStatusLine(status_line, st) && st.has_wpos)
                            emit livePosition(st.wpos_x, st.wpos_y);
                    }
                }
            }
        }

        if (!sendCommandAndOptionalAck(serial, rx_accum,
                                       normalizeCommands(device_->after_job_command), err)) {
            emit finished(false, err);
            finish_and_cleanup();
            return;
        }

        if (restore_10) {
            const QByteArray cmd = QByteArray("$10=")
                                       + QByteArray::number(int(grbl_settings_cache_.value(10)))
                                       + QByteArray("\n");
            (void)sendAndWaitAck(serial, rx_accum, cmd, err);
        }
        if (restore_32) {
            const QByteArray cmd = QByteArray("$32=")
                                       + QByteArray::number(int(grbl_settings_cache_.value(32)))
                                       + QByteArray("\n");
            (void)sendAndWaitAck(serial, rx_accum, cmd, err);
        }

        emit grblSettingsReady(grbl_settings_cache_);
        emit finished(true, {});
        finish_and_cleanup();
        return;
    }

    // Legacy binary/chunk stream for non-GCode protocols.
    qint64 sent = 0;
    const qint64 total = payload_.size();
    while (sent < total) {
        if (cancelled_.load()) {
            emit finished(false, QStringLiteral("Anulowano."));
            finish_and_cleanup();
            return;
        }
        while (paused_.load() && !cancelled_.load()) {
            QThread::msleep(50);
        }
        if (cancelled_.load()) {
            emit finished(false, QStringLiteral("Anulowano."));
            finish_and_cleanup();
            return;
        }
        const qint64 chunk = qMin(qint64(2048), total - sent);
        const qint64 n = serial.write(payload_.constData() + sent, chunk);
        if (n <= 0) {
            emit finished(false, trInk("Błąd zapisu na port."));
            finish_and_cleanup();
            return;
        }
        sent += n;
        emit progress(sent, total);
        if (serial.waitForBytesWritten(30000)) {
            rx_accum.append(serial.readAll());
            if (rx_accum.size() > 4096)
                rx_accum = rx_accum.right(2048);
            if (const auto pos = parseLivePositionFromRx(rx_accum, live_protocol_, live_plot_scale_))
                emit livePosition(pos->x, pos->y);
        }
        QThread::msleep(5);
    }

    serial.waitForReadyRead(100);
    emit finished(true, {});
    finish_and_cleanup();
}

} // namespace inkcut
