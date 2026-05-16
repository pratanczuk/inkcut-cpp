// SPDX-License-Identifier: GPL-3.0-or-later

#include "job_history.hpp"

#include "job_export.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QUuid>

namespace inkcut {

namespace {

QString statusToString(JobRunStatus s)
{
    switch (s) {
    case JobRunStatus::Staged:
        return QStringLiteral("staged");
    case JobRunStatus::Approved:
        return QStringLiteral("approved");
    case JobRunStatus::Running:
        return QStringLiteral("running");
    case JobRunStatus::Paused:
        return QStringLiteral("paused");
    case JobRunStatus::Complete:
        return QStringLiteral("complete");
    case JobRunStatus::Cancelled:
        return QStringLiteral("cancelled");
    case JobRunStatus::Error:
        return QStringLiteral("error");
    }
    return QStringLiteral("staged");
}

JobRunStatus statusFromString(const QString& s)
{
    if (s == QLatin1String("approved"))
        return JobRunStatus::Approved;
    if (s == QLatin1String("running"))
        return JobRunStatus::Running;
    if (s == QLatin1String("paused"))
        return JobRunStatus::Paused;
    if (s == QLatin1String("complete"))
        return JobRunStatus::Complete;
    if (s == QLatin1String("cancelled"))
        return JobRunStatus::Cancelled;
    if (s == QLatin1String("error"))
        return JobRunStatus::Error;
    return JobRunStatus::Staged;
}

} // namespace

QVector<JobHistoryEntry> loadJobHistory()
{
    QVector<JobHistoryEntry> out;
    QSettings settings(QStringLiteral("inkcut"), QStringLiteral("gui"));
    const QByteArray raw = settings.value(QStringLiteral("job_history_v2")).toByteArray();
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isArray())
        return out;
    for (const QJsonValue& v : doc.array()) {
        const QJsonObject o = v.toObject();
        JobHistoryEntry e;
        e.id = o.value(QStringLiteral("id")).toString();
        e.time_iso = o.value(QStringLiteral("time")).toString();
        e.started_iso = o.value(QStringLiteral("started")).toString();
        e.ended_iso = o.value(QStringLiteral("ended")).toString();
        e.run_count = o.value(QStringLiteral("run_count")).toInt();
        e.duration_sec = o.value(QStringLiteral("duration_sec")).toInt();
        e.source_path = o.value(QStringLiteral("source")).toString();
        e.source_kind = o.value(QStringLiteral("kind")).toString();
        e.status = statusFromString(o.value(QStringLiteral("status")).toString());
        e.bytes_sent = o.value(QStringLiteral("bytes_sent")).toInteger();
        e.error_message = o.value(QStringLiteral("error")).toString();
        const QJsonObject bb = o.value(QStringLiteral("bounds")).toObject();
        if (!bb.isEmpty()) {
            e.bounds = QRectF(bb.value(QStringLiteral("x")).toDouble(),
                              bb.value(QStringLiteral("y")).toDouble(),
                              bb.value(QStringLiteral("width")).toDouble(),
                              bb.value(QStringLiteral("height")).toDouble());
        }
        const QByteArray st = o.value(QStringLiteral("settings_json")).toString().toUtf8();
        importJobDocumentJson(st, e.settings, nullptr, nullptr, nullptr, nullptr);
        out.push_back(e);
    }
    return out;
}

void saveJobHistory(const QVector<JobHistoryEntry>& entries)
{
    QJsonArray arr;
    for (const JobHistoryEntry& e : entries) {
        QJsonObject o;
        o.insert(QStringLiteral("id"), e.id);
        o.insert(QStringLiteral("time"), e.time_iso);
        o.insert(QStringLiteral("started"), e.started_iso);
        o.insert(QStringLiteral("ended"), e.ended_iso);
        o.insert(QStringLiteral("run_count"), e.run_count);
        o.insert(QStringLiteral("duration_sec"), e.duration_sec);
        o.insert(QStringLiteral("source"), e.source_path);
        o.insert(QStringLiteral("kind"), e.source_kind);
        o.insert(QStringLiteral("status"), statusToString(e.status));
        o.insert(QStringLiteral("bytes_sent"), e.bytes_sent);
        o.insert(QStringLiteral("error"), e.error_message);
        QJsonObject bb;
        bb.insert(QStringLiteral("x"), e.bounds.x());
        bb.insert(QStringLiteral("y"), e.bounds.y());
        bb.insert(QStringLiteral("width"), e.bounds.width());
        bb.insert(QStringLiteral("height"), e.bounds.height());
        o.insert(QStringLiteral("bounds"), bb);
        const QString json =
            exportJobDocumentToJson(e.source_path, e.source_kind, e.bounds, e.settings);
        o.insert(QStringLiteral("settings_json"), json);
        arr.append(o);
    }
    QSettings settings(QStringLiteral("inkcut"), QStringLiteral("gui"));
    settings.setValue(QStringLiteral("job_history_v2"),
                      QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
}

void appendJobHistoryEntry(const JobHistoryEntry& entry)
{
    QVector<JobHistoryEntry> list = loadJobHistory();
    list.prepend(entry);
    while (list.size() > 64)
        list.removeLast();
    saveJobHistory(list);
}

void removeJobHistoryEntry(const QString& id)
{
    if (id.isEmpty())
        return;
    QVector<JobHistoryEntry> list = loadJobHistory();
    for (int i = list.size() - 1; i >= 0; --i) {
        if (list[i].id == id)
            list.removeAt(i);
    }
    saveJobHistory(list);
}

} // namespace inkcut
