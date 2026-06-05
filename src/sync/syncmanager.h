#pragma once
#include <QObject>
#include <QString>
#include <QNetworkAccessManager>

enum class CloudProvider {
    None,
    GoogleDrive,
    OneDrive,
    Dropbox,
    Mega,
    Box,
    SMB,
    FTP
};

struct SyncConfig {
    CloudProvider provider = CloudProvider::None;
    // OAuth tokens (Google, OneDrive, Dropbox, Box)
    QString accessToken;
    QString refreshToken;
    // SMB / FTP
    QString host;
    int     port = 0;
    QString username;
    QString password;
    QString remotePath;
    // local
    QString localDbPath;
};

class SyncManager : public QObject {
    Q_OBJECT
public:
    explicit SyncManager(QObject* parent = nullptr);

    void configure(const SyncConfig& cfg);
    SyncConfig config() const { return m_cfg; }

    void syncNow();        // pull→merge→push
    void push();
    void pull();

    static QString providerName(CloudProvider p);
    static QString providerOAuthUrl(CloudProvider p, const QString& clientId);

signals:
    void syncStarted();
    void syncProgress(int pct, const QString& message);
    void syncFinished(bool success, const QString& error = {});

private:
    void syncGDrive();
    void syncOneDrive();
    void syncDropbox();
    void syncBox();
    void syncFtp();
    void syncSmb();

    SyncConfig            m_cfg;
    QNetworkAccessManager m_nam;
};
