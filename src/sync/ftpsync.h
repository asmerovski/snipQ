#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include "syncmanager.h"

class FtpSync : public QObject {
    Q_OBJECT
public:
    FtpSync(const SyncConfig& cfg, QNetworkAccessManager* nam, QObject* parent = nullptr);
    bool upload();
    bool download();

signals:
    void progress(int pct, const QString& msg);

private:
    SyncConfig              m_cfg;
    QNetworkAccessManager*  m_nam;
};
