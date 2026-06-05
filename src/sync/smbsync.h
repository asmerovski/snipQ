#pragma once
#include <QObject>
#include "syncmanager.h"

// SMB sync uses OS-level UNC path (\\host\share\path) on Windows,
// or smbclient / GVFS mount on Linux/macOS.
class SmbSync : public QObject {
    Q_OBJECT
public:
    SmbSync(const SyncConfig& cfg, QObject* parent = nullptr);
    bool upload();
    bool download();

signals:
    void progress(int pct, const QString& msg);

private:
    SyncConfig m_cfg;
};
