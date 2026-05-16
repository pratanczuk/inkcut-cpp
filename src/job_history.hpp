// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "job_model.hpp"

#include <QRectF>
#include <QVector>

namespace inkcut {

enum class JobRunStatus { Staged, Approved, Running, Paused, Complete, Cancelled, Error };

struct JobHistoryEntry {
    QString id;
    QString time_iso;
    QString started_iso;
    QString ended_iso;
    QString source_path;
    QString source_kind;
    JobRunStatus status = JobRunStatus::Staged;
    PlotJobSettings settings;
    QRectF bounds;
    qint64 bytes_sent = 0;
    int run_count = 0;
    int duration_sec = 0;
    QString error_message;
};

QVector<JobHistoryEntry> loadJobHistory();
void saveJobHistory(const QVector<JobHistoryEntry>& entries);
void appendJobHistoryEntry(const JobHistoryEntry& entry);
void removeJobHistoryEntry(const QString& id);

} // namespace inkcut
