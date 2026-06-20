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

// Helper: perform a JSON POST/PATCH and return response body
static QByteArray syncJsonRequest(QNetworkAccessManager& nam, const QString& method,
                                   const QUrl& url, const QByteArray& body,
                                   const QString& bearer)
{
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!bearer.isEmpty())
        req.setRawHeader("Authorization", ("Bearer " + bearer).toUtf8());
    QEventLoop loop;
    QNetworkReply* reply = nullptr;
    if (method == "POST")
        reply = nam.post(req, body);
    else if (method == "PATCH")
        reply = nam.sendCustomRequest(req, "PATCH", body);
    else
        reply = nam.post(req, body);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    QByteArray data = reply->readAll();
    reply->deleteLater();
    return data;
}

void SyncManager::syncGDrive() {
    emit syncProgress(10, "Connecting to Google Drive…");
    if (m_cfg.accessToken.isEmpty()) {
        emit syncFinished(false, "No Google Drive access token. Please authenticate first.");
        return;
    }

    // ── Step 1: find or create the "snipQ" folder in My Drive ────────────────
    emit syncProgress(20, "Locating snipQ folder…");
    QUrl folderSearchUrl(
        "https://www.googleapis.com/drive/v3/files"
        "?q=name%3D'snipQ'+and+mimeType%3D'application%2Fvnd.google-apps.folder'"
        "+and+'root'+in+parents+and+trashed%3Dfalse"
        "&fields=files(id)&pageSize=1");
    QByteArray folderData = syncGet(m_nam, folderSearchUrl, m_cfg.accessToken);
    auto folderDoc = QJsonDocument::fromJson(folderData);
    QString folderId;
    if (folderDoc.isObject()) {
        auto arr = folderDoc.object()["files"].toArray();
        if (!arr.isEmpty())
            folderId = arr[0].toObject()["id"].toString();
    }
    if (folderId.isEmpty()) {
        // Create the folder
        QJsonObject meta;
        meta["name"]     = "snipQ";
        meta["mimeType"] = "application/vnd.google-apps.folder";
        QByteArray resp = syncJsonRequest(m_nam, "POST",
            QUrl("https://www.googleapis.com/drive/v3/files"),
            QJsonDocument(meta).toJson(QJsonDocument::Compact),
            m_cfg.accessToken);
        auto respDoc = QJsonDocument::fromJson(resp);
        if (respDoc.isObject())
            folderId = respDoc.object()["id"].toString();
    }
    if (folderId.isEmpty()) {
        emit syncFinished(false, "Could not find or create snipQ folder on Google Drive.");
        return;
    }

    // ── Step 2: find existing snipQ_db.json inside that folder ───────────────
    emit syncProgress(35, "Checking for existing backup…");
    QUrl fileSearchUrl(
        QStringLiteral("https://www.googleapis.com/drive/v3/files"
                       "?q=name%3D'snipQ_db.json'+and+'%1'+in+parents+and+trashed%3Dfalse"
                       "&fields=files(id)&pageSize=1").arg(folderId));
    QByteArray fileListData = syncGet(m_nam, fileSearchUrl, m_cfg.accessToken);
    auto fileListDoc = QJsonDocument::fromJson(fileListData);
    QString fileId;
    if (fileListDoc.isObject()) {
        auto arr = fileListDoc.object()["files"].toArray();
        if (!arr.isEmpty())
            fileId = arr[0].toObject()["id"].toString();
    }

    // ── Step 3: read local DB ─────────────────────────────────────────────────
    QFile f(m_cfg.localDbPath);
    if (!f.open(QIODevice::ReadOnly)) {
        emit syncFinished(false, "Could not read local DB."); return;
    }
    QByteArray dbBytes = f.readAll();

    emit syncProgress(60, "Uploading…");

    bool ok = false;
    if (fileId.isEmpty()) {
        // ── New file: multipart upload (metadata + content) ───────────────────
        // Build multipart/related body
        const QString boundary = "snipq_boundary_xyz987";
        QJsonObject meta2;
        meta2["name"] = "snipQ_db.json";
        meta2["parents"] = QJsonArray{ folderId };
        QByteArray metaJson = QJsonDocument(meta2).toJson(QJsonDocument::Compact);

        QByteArray body;
        body += "--" + boundary.toUtf8() + "\r\n";
        body += "Content-Type: application/json; charset=UTF-8\r\n\r\n";
        body += metaJson + "\r\n";
        body += "--" + boundary.toUtf8() + "\r\n";
        body += "Content-Type: application/octet-stream\r\n\r\n";
        body += dbBytes + "\r\n";
        body += "--" + boundary.toUtf8() + "--";

        QNetworkRequest req(QUrl("https://www.googleapis.com/upload/drive/v3/files"
                                  "?uploadType=multipart"));
        req.setHeader(QNetworkRequest::ContentTypeHeader,
                      ("multipart/related; boundary=" + boundary).toUtf8());
        req.setRawHeader("Authorization", ("Bearer " + m_cfg.accessToken).toUtf8());
        QEventLoop loop;
        auto* reply = m_nam.post(req, body);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();
        ok = (code == 200 || code == 201);
    } else {
        // ── Existing file: simple media update ────────────────────────────────
        QUrl updateUrl(QStringLiteral(
            "https://www.googleapis.com/upload/drive/v3/files/%1?uploadType=media").arg(fileId));
        ok = syncPut(m_nam, updateUrl, dbBytes, m_cfg.accessToken);
    }

    emit syncProgress(100, ok ? "Done." : "Failed.");
    emit syncFinished(ok, ok ? QString() : "Google Drive upload failed.");
}

// ── OneDrive ─────────────────────────────────────────────────────────────────

void SyncManager::syncOneDrive() {
    emit syncProgress(10, "Connecting to OneDrive…");
    if (m_cfg.accessToken.isEmpty()) {
        emit syncFinished(false, "No OneDrive access token."); return;
    }

    // ── Step 1: ensure the snipQ folder exists in the user's personal drive ──
    emit syncProgress(25, "Locating snipQ folder…");
    // The Graph API will create the folder on PUT if it doesn't exist when using
    // the path-based upload URL; we explicitly create it first for reliability.
    QUrl mkdirUrl("https://graph.microsoft.com/v1.0/me/drive/root/children");
    QJsonObject folderMeta;
    folderMeta["name"]                          = "snipQ";
    folderMeta["folder"]                        = QJsonObject();
    folderMeta["@microsoft.graph.conflictBehavior"] = "replace"; // no error if exists
    syncJsonRequest(m_nam, "POST", mkdirUrl,
                    QJsonDocument(folderMeta).toJson(QJsonDocument::Compact),
                    m_cfg.accessToken);
    // (We ignore the response; if it already exists the API just returns the
    //  existing item. Either way the folder is present.)

    // ── Step 2: read local DB ─────────────────────────────────────────────────
    QFile f(m_cfg.localDbPath);
    if (!f.open(QIODevice::ReadOnly)) {
        emit syncFinished(false, "Could not read local DB."); return;
    }
    QByteArray dbBytes = f.readAll();

    emit syncProgress(60, "Uploading…");
    // PUT into the personal snipQ subfolder
    QUrl url("https://graph.microsoft.com/v1.0/me/drive/root:/snipQ/snipQ_db.json:/content");
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
    // Upload into the user's personal /snipQ/ folder (Dropbox creates it automatically)
    QNetworkRequest req(QUrl("https://content.dropboxapi.com/2/files/upload"));
    req.setRawHeader("Authorization", ("Bearer " + m_cfg.accessToken).toUtf8());
    req.setRawHeader("Dropbox-API-Arg",
        "{\"path\":\"/snipQ/snipQ_db.json\","
        "\"mode\":\"overwrite\","
        "\"autorename\":false,"
        "\"mute\":false}");
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

    // ── Step 1: find or create the snipQ folder under root (id "0") ──────────
    emit syncProgress(25, "Locating snipQ folder…");
    QByteArray rootItems = syncGet(m_nam,
        QUrl("https://api.box.com/2.0/folders/0/items?fields=id,name,type"),
        m_cfg.accessToken);
    auto rootDoc = QJsonDocument::fromJson(rootItems);
    QString snipQFolderId;
    if (rootDoc.isObject()) {
        for (auto entry : rootDoc.object()["entries"].toArray()) {
            auto obj = entry.toObject();
            if (obj["type"].toString() == "folder" &&
                obj["name"].toString() == "snipQ") {
                snipQFolderId = obj["id"].toString();
                break;
            }
        }
    }
    if (snipQFolderId.isEmpty()) {
        // Create the folder
        QJsonObject body;
        body["name"]   = "snipQ";
        body["parent"] = QJsonObject{ {"id", "0"} };
        QByteArray resp = syncJsonRequest(m_nam, "POST",
            QUrl("https://api.box.com/2.0/folders"),
            QJsonDocument(body).toJson(QJsonDocument::Compact),
            m_cfg.accessToken);
        auto respDoc = QJsonDocument::fromJson(resp);
        if (respDoc.isObject())
            snipQFolderId = respDoc.object()["id"].toString();
    }
    if (snipQFolderId.isEmpty()) {
        emit syncFinished(false, "Could not find or create snipQ folder on Box."); return;
    }

    // ── Step 2: check if snipQ_db.json already exists inside that folder ──────
    emit syncProgress(40, "Checking for existing backup…");
    QByteArray folderItems = syncGet(m_nam,
        QUrl(QStringLiteral("https://api.box.com/2.0/folders/%1/items"
                             "?fields=id,name,type").arg(snipQFolderId)),
        m_cfg.accessToken);
    auto folderDoc = QJsonDocument::fromJson(folderItems);
    QString existingFileId;
    if (folderDoc.isObject()) {
        for (auto entry : folderDoc.object()["entries"].toArray()) {
            auto obj = entry.toObject();
            if (obj["type"].toString() == "file" &&
                obj["name"].toString() == "snipQ_db.json") {
                existingFileId = obj["id"].toString();
                break;
            }
        }
    }

    emit syncProgress(55, "Uploading…");
    QFile* file = new QFile(m_cfg.localDbPath);
    if (!file->open(QIODevice::ReadOnly)) {
        delete file;
        emit syncFinished(false, "Could not read local DB."); return;
    }

    int code = 0;
    if (existingFileId.isEmpty()) {
        // ── New file upload ───────────────────────────────────────────────────
        auto* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
        QHttpPart attrPart;
        attrPart.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        attrPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                           "form-data; name=\"attributes\"");
        attrPart.setBody(
            QJsonDocument(QJsonObject{
                {"name", "snipQ_db.json"},
                {"parent", QJsonObject{{"id", snipQFolderId}}}
            }).toJson(QJsonDocument::Compact));
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
        code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();
    } else {
        // ── New version of existing file ──────────────────────────────────────
        auto* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
        QHttpPart attrPart;
        attrPart.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        attrPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                           "form-data; name=\"attributes\"");
        attrPart.setBody("{}");
        multiPart->append(attrPart);
        QHttpPart filePart;
        filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                           "form-data; name=\"file\"; filename=\"snipQ_db.json\"");
        filePart.setBodyDevice(file);
        file->setParent(multiPart);
        multiPart->append(filePart);

        QNetworkRequest req(QUrl(QStringLiteral(
            "https://upload.box.com/api/2.0/files/%1/content").arg(existingFileId)));
        req.setRawHeader("Authorization", ("Bearer " + m_cfg.accessToken).toUtf8());
        QEventLoop loop;
        auto* reply = m_nam.post(req, multiPart);
        multiPart->setParent(reply);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();
    }

    bool ok = (code == 200 || code == 201);
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
