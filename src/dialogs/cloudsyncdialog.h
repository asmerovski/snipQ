#pragma once
#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QProgressBar>
#include <QStackedWidget>
#include <QPushButton>
#include <QMap>
#include "../sync/syncmanager.h"
#include "../database.h"

class CloudSyncDialog : public QDialog {
    Q_OBJECT
public:
    explicit CloudSyncDialog(Database* db, SyncManager* sync, QWidget* parent = nullptr);

private slots:
    void onProviderChanged(int idx);
    void onSyncNow();
    void onAuthenticate();
    void onSyncProgress(int pct, const QString& msg);
    void onSyncFinished(bool ok, const QString& err);

private:
    QWidget* buildOAuthPage(int providerIndex, CloudProvider p);
    QWidget* buildFtpPage();
    QWidget* buildSmbPage();

    Database*       m_db;
    SyncManager*    m_sync;

    QComboBox*      m_providerCombo;
    QStackedWidget* m_stack;
    QProgressBar*   m_progress;
    QLabel*         m_statusLabel;
    QPushButton*    m_syncBtn;

    // One token field per OAuth provider (indexed 0-3)
    QMap<int, QLineEdit*> m_oauthTokens;

    // FTP fields
    QLineEdit* m_ftpHost;
    QLineEdit* m_ftpPort;
    QLineEdit* m_ftpUser;
    QLineEdit* m_ftpPass;
    QLineEdit* m_ftpPath;
    // SMB fields
    QLineEdit* m_smbHost;
    QLineEdit* m_smbUser;
    QLineEdit* m_smbPass;
    QLineEdit* m_smbPath;
};
