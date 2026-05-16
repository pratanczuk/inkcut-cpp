// SPDX-License-Identifier: GPL-3.0-or-later

#include "plugin_loader.hpp"

#include "device_plugin.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QPluginLoader>
#include <QSet>
#include <QStandardPaths>

namespace inkcut {

namespace {

void append_unique_dir(QStringList& dirs, const QString& path)
{
    if (path.isEmpty())
        return;
    const QFileInfo fi(path);
    const QString abs = fi.absoluteFilePath();
    if (!fi.exists() || !fi.isDir())
        return;
    if (!dirs.contains(abs))
        dirs.push_back(abs);
}

QStringList plugin_search_directories()
{
    QStringList dirs;

    if (QCoreApplication::instance()) {
        const QDir app(QCoreApplication::applicationDirPath());
        append_unique_dir(dirs, app.filePath(QStringLiteral("plugins")));
    }

    const QByteArray env = qgetenv("INKCUT_PLUGIN_PATH");
    if (!env.isEmpty()) {
        const QStringList parts = QString::fromLocal8Bit(env).split(QLatin1Char(':'), Qt::SkipEmptyParts);
        for (const QString& p : parts)
            append_unique_dir(dirs, p.trimmed());
    }

    append_unique_dir(dirs, QStringLiteral("/usr/lib/inkcut/plugins"));
    append_unique_dir(dirs, QStringLiteral("/usr/local/lib/inkcut/plugins"));

    const QStringList stdloc =
        QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    for (const QString& base : stdloc) {
        append_unique_dir(dirs, QDir(base).filePath(QStringLiteral("plugins")));
    }

    return dirs;
}

} // namespace

DevicePluginLoader::DevicePluginLoader() = default;

DevicePluginLoader::~DevicePluginLoader() = default;

void DevicePluginLoader::rescan(QStringList* errors)
{
    loaders_.clear();
    instances_.clear();

    const QStringList dirs = plugin_search_directories();
    const QString suffix = QStringLiteral(".so");

    QSet<QString> seen_files;

    for (const QString& dirPath : dirs) {
        QDir dir(dirPath);
        const QFileInfoList entries =
            dir.entryInfoList(QDir::Files | QDir::Readable, QDir::Name);
        for (const QFileInfo& fi : entries) {
            const QString path = fi.absoluteFilePath();
#ifdef Q_OS_WIN
            // Qt plugins on Windows use `.dll`; keep portable builds happy.
            if (!fi.fileName().endsWith(QLatin1String(".dll"), Qt::CaseInsensitive))
                continue;
#else
            if (!fi.fileName().endsWith(suffix))
                continue;
#endif
            if (seen_files.contains(path))
                continue;
            seen_files.insert(path);

            auto loader = std::make_unique<QPluginLoader>(path);
            QObject* obj = loader->instance();
            if (!obj) {
                if (errors)
                    errors->append(QStringLiteral("%1: %2").arg(path, loader->errorString()));
                continue;
            }
            auto* plug = qobject_cast<DevicePlugin*>(obj);
            if (!plug) {
                if (errors)
                    errors->append(QStringLiteral("%1: nie jest wtyczką DevicePlugin").arg(path));
                continue;
            }

            instances_.push_back(plug);
            loaders_.push_back(std::move(loader));
        }
    }
}

DevicePlugin* DevicePluginLoader::findById(const QString& plugin_id) const
{
    if (plugin_id.isEmpty())
        return nullptr;
    for (DevicePlugin* p : instances_) {
        if (p && p->pluginId() == plugin_id)
            return p;
    }
    return nullptr;
}

} // namespace inkcut
