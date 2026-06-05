#include "smbsync.h"
#include <QFile>
#include <QDir>
#include <QProcess>
#include <QDebug>

SmbSync::SmbSync(const SyncConfig& cfg, QObject* parent)
    : QObject(parent), m_cfg(cfg) {}

bool SmbSync::upload() {
    emit progress(10, QStringLiteral("Connecting to SMB %1…").arg(m_cfg.host));

#ifdef Q_OS_WIN
    // On Windows: copy file to UNC path directly
    QString uncPath = QStringLiteral("\\\\%1\\%2").arg(m_cfg.host, m_cfg.remotePath);
    uncPath.replace('/', '\\');
    QFile src(m_cfg.localDbPath);
    if (!src.open(QIODevice::ReadOnly)) {
        emit progress(0, "Cannot read local DB.");
        return false;
    }
    emit progress(40, "Copying to SMB share…");
    bool ok = src.copy(uncPath + "\\snipQ_db.json");
    emit progress(100, ok ? "SMB copy done." : "SMB copy failed.");
    return ok;
#else
    // On Linux/macOS: use smbclient
    emit progress(30, "Using smbclient…");
    QStringList args = {
        QStringLiteral("//%1/%2").arg(m_cfg.host, m_cfg.remotePath),
        "-U", QStringLiteral("%1%%%2").arg(m_cfg.username, m_cfg.password),
        "-c", QStringLiteral("put \"%1\" snipQ_db.json").arg(m_cfg.localDbPath)
    };
    QProcess proc;
    proc.start("smbclient", args);
    proc.waitForFinished(30000);
    bool ok = (proc.exitCode() == 0);
    emit progress(100, ok ? "SMB upload done." : "SMB upload failed.");
    return ok;
#endif
}

bool SmbSync::download() {
    emit progress(10, "Downloading from SMB…");

#ifdef Q_OS_WIN
    QString uncPath = QStringLiteral("\\\\%1\\%2\\snipQ_db.json")
                        .arg(m_cfg.host, m_cfg.remotePath);
    uncPath.replace('/', '\\');
    QString dest = m_cfg.localDbPath + ".remote";
    QFile::remove(dest);
    bool ok = QFile::copy(uncPath, dest);
    emit progress(100, ok ? "Done." : "Failed.");
    return ok;
#else
    QStringList args = {
        QStringLiteral("//%1/%2").arg(m_cfg.host, m_cfg.remotePath),
        "-U", QStringLiteral("%1%%%2").arg(m_cfg.username, m_cfg.password),
        "-c", QStringLiteral("get snipQ_db.json \"%1\"").arg(m_cfg.localDbPath + ".remote")
    };
    QProcess proc;
    proc.start("smbclient", args);
    proc.waitForFinished(30000);
    bool ok = (proc.exitCode() == 0);
    emit progress(100, ok ? "Done." : "Failed.");
    return ok;
#endif
}
