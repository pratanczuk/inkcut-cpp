// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "protocols.hpp"
#include "job_model.hpp"

#include <QByteArray>
#include <QObject>
#include <QPointF>
#include <QMap>
#include <QStringList>
#include <atomic>

class QSerialPort;

namespace inkcut {

struct DeviceSetup;

/// Asynchroniczna wysyłka z pauzą / anulowaniem i postępem (live plot).
class PlotSendWorker final : public QObject {
    Q_OBJECT
public:
    explicit PlotSendWorker(QObject* parent = nullptr);

    void setPayload(QByteArray payload);
    void setDevice(const DeviceSetup& device);
    void setLiveParse(PlotProtocol protocol, double plot_scale);
    void setProtocolSettings(const ProtocolSettings& protocol);

    void requestPause();
    void requestResume();
    void requestCancel();

    bool isPaused() const { return paused_.load(); }
    bool isCancelled() const { return cancelled_.load(); }

public slots:
    void run();

signals:
    void progress(qint64 sent, qint64 total);
    void livePosition(double x, double y);
    void grblSettingsReady(const QMap<int, double>& settings);
    void finished(bool success, const QString& error);
    void pausedChanged(bool paused);

private:
    QByteArray payload_;
    DeviceSetup* device_ = nullptr;
    ProtocolSettings protocol_;
    PlotProtocol live_protocol_ = PlotProtocol::HPGL;
    double live_plot_scale_ = 1021.0 / 90.0;
    QMap<int, double> grbl_settings_cache_;
    std::atomic<bool> paused_{false};
    std::atomic<bool> cancelled_{false};
    std::atomic<bool> resume_requested_{false};
};

} // namespace inkcut
