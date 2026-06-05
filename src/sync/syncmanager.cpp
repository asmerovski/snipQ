#include "syncmanager.h"
#include "ftpsync.h"
#include "smbsync.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QHttpMultiPart>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QEventLoop>
#include <QDebug>

SyncManager::SyncManager(QObject* parent) : QObject(parent) {}

void SyncManager::configure(const SyncConfig& cfg) { m_cfg = cfg; }

void SyncManager::syncNow() {
    emit syncStarted();
    switch (m_cfg.provider) {
        case CloudProvider::GoogleDrive: syncGDrive();   break;
        case CloudProvider::OneDrive:    syncOneDrive(); break;
        case CloudProvider::Dropbox:     syncDropbox();  break;
        case CloudProvider::Box:         syncBox();      break;
        case CloudProvider::FTP:         syncFtp();      break;
        case CloudProvider::SMB:         syncSmb();      break;
        default:
            emit syncFinished(false, "No provider configured.");
    }
}

void SyncManager::push() { syncNow(); }
void SyncManager::pull() { syncNow(); }

QString SyncManager::providerName(CloudProvider p) {
    switch (p) {
        case CloudProvider::GoogleDrive: return "Google Drive";
        case CloudProvider::OneDrive:    return "OneDrive";
        case CloudProvider::Dropbox:     return "Dropbox";
        case CloudProvider::Mega:        return "MEGA";
        case CloudProvider::Box:         return "Box";
        case CloudProvider::SMB:         return "SMB / Samba";
        case CloudProvider::FTP:         return "FTP / FTPS";
        default:                         return "None";
    }
}

// ── Generic helper: upload file bytes via PUT ─────────────────────────────────

static QByteArray syncGet(QNetworkAccessManager& nam, const QUrl& url,
                           const QString& bearer) {
    QNetworkRequest req(url);
    if (!bearer.isEmpty()) req.setRawHeader("Authorization", ("Bearer " + bearer).toUtf8());
    QEventLoop loop;
    auto* reply = nam.get(req);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    QByteArray data = reply->readAll();
    reply->deleteLater();
    return data;
}

static bool syncPut(QNetworkAccessManager& nam, const QUrl& url,
                    const QByteArray& data, const QString& bearer) {
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
    if (!bearer.isEmpty()) req.setRawHeader("Authorization", ("Bearer " + bearer).toUtf8());
    QEventLoop loop;
    auto* reply = nam.put(req, data);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();
    return status >= 200 && status < 300;
}

// ── Google Drive ─────────────────────────────────────────────────────────────

void SyncManager::syncGDrive() {
    // Upload using Drive API simple upload endpoint
    emit syncProgress(10, "Connecting to Google Drive…");
    if (m_cfg.accessToken.isEmpty()) {
        emit syncFinished(false, "No Google Drive access token. Please authenticate first.");
        return;
    }
    // Search for existing file
    QUrl searchUrl("https://www.googleapis.com/drive/v3/files"
                   "?q=name%3D'snipQ_db.json'+and+trashed%3Dfalse"
                   "&fields=files(id,name,modifiedTime)");
    QByteArray listData = syncGet(m_nam, searchUrl, m_cfg.accessToken);
    auto listDoc = QJsonDocument::fromJson(listData);
    QString fileId;
    if (listDoc.isObject()) {
        auto files = listDoc.object()["files"].toArray();
        if (!files.isEmpty()) fileId = files[0].toObject()["id"].toString();
    }
    // Read local DB file
    QFile f(m_cfg.localDbPath);
    if (!f.open(QIODevice::ReadOnly)) {
        emit syncFinished(false, "Could not read local DB."); return;
    }
    QByteArray dbBytes = f.readAll();

    emit syncProgress(50, "Uploading…");
    QUrl uploadUrl;
    if (fileId.isEmpty()) {
        uploadUrl = QUrl("https://www.googleapis.com/upload/drive/v3/files?uploadType=media");
        // In real app: send metadata first, then upload
    } else {
        uploadUrl = QUrl(QStringLiteral("https://www.googleapis.com/upload/drive/v3/files/%1?uploadType=media").arg(fileId));
    }
    bool ok = syncPut(m_nam, uploadUrl, dbBytes, m_cfg.accessToken);
    emit syncProgress(100, ok ? "Done." : "Failed.");
    emit syncFinished(ok, ok ? QString() : "Upload failed.");
}

// ── OneDrive ─────────────────────────────────────────────────────────────────

void SyncManager::syncOneDrive() {
    emit syncProgress(10, "Connecting to OneDrive…");
    if (m_cfg.accessToken.isEmpty()) {
        emit syncFinished(false, "No OneDrive access token."); return;
    }
    QFile f(m_cfg.localDbPath);
    if (!f.open(QIODevice::ReadOnly)) {
        emit syncFinished(false, "Could not read local DB."); return;
    }
    QByteArray dbBytes = f.readAll();
    emit syncProgress(50, "Uploading…");
    // PUT to personal root
    QUrl url("https://graph.microsoft.com/v1.0/me/drive/root:/snipQ_db.json:/content");
    bool ok = syncPut(m_nam, url, dbBytes, m_cfg.accessToken);
    emit syncProgress(100, ok ? "Done." : "Failed.");
    emit syncFinished(ok, ok ? QString() : "OneDrive upload failed.");
}

// ── Dropbox ──────────────────────────────────────────────────────────────────

void SyncManager::syncDropbox() {
    emit syncProgress(10, "Connecting to Dropbox…");
    if (m_cfg.accessToken.isEmpty()) {
        emit syncFinished(false, "No Dropbox access token."); return;
    }
    QFile f(m_cfg.localDbPath);
    if (!f.open(QIODevice::ReadOnly)) {
        emit syncFinished(false, "Could not read local DB."); return;
    }
    QByteArray dbBytes = f.readAll();
    emit syncProgress(50, "Uploading…");
    // Dropbox upload
    QNetworkRequest req(QUrl("https://content.dropboxapi.com/2/files/upload"));
    req.setRawHeader("Authorization", ("Bearer " + m_cfg.accessToken).toUtf8());
    req.setRawHeader("Dropbox-API-Arg",
        "{\"path\":\"/snipQ_db.json\",\"mode\":\"overwrite\"}");
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
    QEventLoop loop;
    auto* reply = m_nam.post(req, dbBytes);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();
    bool ok = (code == 200);
    emit syncProgress(100, ok ? "Done." : "Failed.");
    emit syncFinished(ok, ok ? QString() : "Dropbox upload failed.");
}

// ── Box ───────────────────────────────────────────────────────────────────────

void SyncManager::syncBox() {
    emit syncProgress(10, "Connecting to Box…");
    if (m_cfg.accessToken.isEmpty()) {
        emit syncFinished(false, "No Box access token."); return;
    }
    emit syncProgress(50, "Uploading…");
    // Box uses multipart upload
    QFile* file = new QFile(m_cfg.localDbPath);
    if (!file->open(QIODevice::ReadOnly)) {
        delete file;
        emit syncFinished(false, "Could not read local DB."); return;
    }
    auto* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    QHttpPart attrPart;
    attrPart.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    attrPart.setHeader(QNetworkRequest::ContentDispositionHeader, "form-data; name=\"attributes\"");
    attrPart.setBody("{\"name\":\"snipQ_db.json\",\"parent\":{\"id\":\"0\"}}");
    multiPart->append(attrPart);
    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       "form-data; name=\"file\"; filename=\"snipQ_db.json\"");
    filePart.setBodyDevice(file);
    file->setParent(multiPart);
    multiPart->append(filePart);

    QNetworkRequest req(QUrl("https://upload.box.com/api/2.0/files/content"));
    req.setRawHeader("Authorization", ("Bearer " + m_cfg.accessToken).toUtf8());
    QEventLoop loop;
    auto* reply = m_nam.post(req, multiPart);
    multiPart->setParent(reply);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();
    bool ok = (code == 201 || code == 200);
    emit syncProgress(100, ok ? "Done." : "Failed.");
    emit syncFinished(ok, ok ? QString() : "Box upload failed.");
}

// ── FTP ───────────────────────────────────────────────────────────────────────

void SyncManager::syncFtp() {
    FtpSync ftp(m_cfg, &m_nam, this);
    connect(&ftp, &FtpSync::progress, this, &SyncManager::syncProgress);
    bool ok = ftp.upload();
    emit syncFinished(ok, ok ? QString() : "FTP upload failed.");
}

// ── SMB ───────────────────────────────────────────────────────────────────────

void SyncManager::syncSmb() {
    SmbSync smb(m_cfg, this);
    connect(&smb, &SmbSync::progress, this, &SyncManager::syncProgress);
    bool ok = smb.upload();
    emit syncFinished(ok, ok ? QString() : "SMB copy failed.");
}
