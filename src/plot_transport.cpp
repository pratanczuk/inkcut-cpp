// SPDX-License-Identifier: GPL-3.0-or-later

#include "plot_transport.hpp"

#include "serial_sender.hpp"

#include <QFile>
#include <QProcess>
#include <QSerialPort>

namespace inkcut {

namespace {

bool spoolToCupsPrinter(const QString& file_path, const QString& printer_name, QString* err)
{
    if (printer_name.trimmed().isEmpty()) {
        if (err)
            *err = QStringLiteral("Podaj nazwę drukarki CUPS (pole „Drukarka”).");
        return false;
    }

    QProcess proc;
    QStringList args;
    args << QStringLiteral("-d") << printer_name.trimmed() << file_path;
    proc.start(QStringLiteral("lp"), args);
    if (!proc.waitForStarted(5000)) {
        if (err)
            *err = QStringLiteral("Nie można uruchomić „lp” — zainstaluj CUPS lub zapisz plik ręcznie.");
        return false;
    }
    if (!proc.waitForFinished(120000)) {
        proc.kill();
        if (err)
            *err = QStringLiteral("Przekroczono czas oczekiwania na lp.");
        return false;
    }
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        if (err)
            *err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        if (err && err->isEmpty())
            *err = QStringLiteral("lp zakończył się kodem %1").arg(proc.exitCode());
        return false;
    }
    return true;
}

} // namespace

TransportResult sendPlotPayload(const QByteArray& payload, const DeviceSetup& device)
{
    TransportResult r;

    switch (device.transport) {
    case PlotTransportKind::FileOutput: {
        const QString path = device.output_path.trimmed();
        if (path.isEmpty()) {
            r.error_message = QStringLiteral("Brak ścieżki pliku wyjściowego.");
            return r;
        }
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            r.error_message = QStringLiteral("Nie można zapisać: %1").arg(path);
            return r;
        }
        r.bytes_written = f.write(payload);
        r.ok = r.bytes_written == payload.size();
        if (!r.ok)
            r.error_message = QStringLiteral("Zapis niekompletny.");
        return r;
    }
    case PlotTransportKind::Printer: {
        QString path = device.output_path.trimmed();
        if (path.isEmpty())
            path = QStringLiteral("/tmp/inkcut_spool.prn");
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            r.error_message = QStringLiteral("Nie można zapisać pliku dla drukarki: %1").arg(path);
            return r;
        }
        r.bytes_written = f.write(payload);
        if (r.bytes_written != payload.size()) {
            r.error_message = QStringLiteral("Zapis niekompletny.");
            return r;
        }
        f.close();

        QString lp_err;
        if (spoolToCupsPrinter(path, device.printer_name, &lp_err)) {
            r.ok = true;
            r.error_message.clear();
        } else {
            r.ok = true;
            r.error_message =
                QStringLiteral("Zapisano %1 — lp nie powiódł się: %2")
                    .arg(path, lp_err);
        }
        return r;
    }
    case PlotTransportKind::SerialPort:
    default: {
        QSerialPort serial;
        SerialOpenOptions opt;
        opt.port_name = device.port_name;
        opt.baud_rate = device.baud_rate;
        if (!open_serial(serial, opt)) {
            r.error_message =
                QStringLiteral("Nie można otworzyć portu %1").arg(device.port_name);
            return r;
        }
        if (!write_all(serial, payload)) {
            r.error_message = QStringLiteral("Zapis na port nie powiódł się.");
            return r;
        }
        r.bytes_written = payload.size();
        r.ok = true;
        return r;
    }
    }
}

} // namespace inkcut
