#include "ftpsync.h"
#include <QFile>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QUrl>

FtpSync::FtpSync(const SyncConfig& cfg, QNetworkAccessManager* nam, QObject* parent)
    : QObject(parent), m_cfg(cfg), m_nam(nam) {}

bool FtpSync::upload() {
    emit progress(10, QStringLiteral("Connecting to FTP %1…").arg(m_cfg.host));

    QFile f(m_cfg.localDbPath);
    if (!f.open(QIODevice::ReadOnly)) {
        emit progress(0, "Cannot read local DB.");
        return false;
    }
    QByteArray data = f.readAll();

    int port = (m_cfg.port > 0) ? m_cfg.port : 21;
    QString path = m_cfg.remotePath.isEmpty() ? "/snipQ_db.json" : m_cfg.remotePath;
    QUrl url;
    url.setScheme("ftp");
    url.setHost(m_cfg.host);
    url.setPort(port);
    url.setPath(path);
    if (!m_cfg.username.isEmpty()) url.setUserName(m_cfg.username);
    if (!m_cfg.password.isEmpty()) url.setPassword(m_cfg.password);

    emit progress(40, "Uploading…");
    QNetworkRequest req(url);
    QEventLoop loop;
    auto* reply = m_nam->put(req, data);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    bool ok = (reply->error() == QNetworkReply::NoError);
    reply->deleteLater();

    emit progress(100, ok ? "FTP upload done." : "FTP upload failed.");
    return ok;
}

bool FtpSync::download() {
    emit progress(10, "Downloading from FTP…");
    int port = (m_cfg.port > 0) ? m_cfg.port : 21;
    QString path = m_cfg.remotePath.isEmpty() ? "/snipQ_db.json" : m_cfg.remotePath;
    QUrl url;
    url.setScheme("ftp");
    url.setHost(m_cfg.host);
    url.setPort(port);
    url.setPath(path);
    if (!m_cfg.username.isEmpty()) url.setUserName(m_cfg.username);
    if (!m_cfg.password.isEmpty()) url.setPassword(m_cfg.password);

    QEventLoop loop;
    auto* reply = m_nam->get(QNetworkRequest(url));
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        reply->deleteLater();
        emit progress(0, "FTP download failed.");
        return false;
    }
    QFile out(m_cfg.localDbPath + ".remote");
    out.open(QIODevice::WriteOnly);
    out.write(reply->readAll());
    reply->deleteLater();
    emit progress(100, "FTP download done.");
    return true;
}
