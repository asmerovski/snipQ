#include "cloudsyncdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>

static const QList<QPair<QString, CloudProvider>> PROVIDERS = {
    {"Google Drive",  CloudProvider::GoogleDrive},
    {"OneDrive",      CloudProvider::OneDrive},
    {"Dropbox",       CloudProvider::Dropbox},
    {"Box",           CloudProvider::Box},
    {"MEGA (manual)", CloudProvider::Mega},
    {"FTP / FTPS",    CloudProvider::FTP},
    {"SMB / Samba",   CloudProvider::SMB},
};

CloudSyncDialog::CloudSyncDialog(Database* db, SyncManager* sync, QWidget* parent)
    : QDialog(parent), m_db(db), m_sync(sync) {
    setWindowTitle("Cloud Sync");
    setMinimumSize(480, 400);

    auto* lay = new QVBoxLayout(this);
    lay->setSpacing(12);
    lay->setContentsMargins(20, 20, 20, 20);

    // Provider selector
    auto* provRow = new QHBoxLayout;
    provRow->addWidget(new QLabel("Provider:", this));
    m_providerCombo = new QComboBox(this);
    for (auto& [name, _] : PROVIDERS) m_providerCombo->addItem(name);
    provRow->addWidget(m_providerCombo, 1);
    lay->addLayout(provRow);

    // Stacked config pages
    m_stack = new QStackedWidget(this);

    // OAuth pages (Google, OneDrive, Dropbox, Box)
    for (int i = 0; i < 4; ++i) m_stack->addWidget(buildOAuthPage(PROVIDERS[i].second));

    // MEGA placeholder
    auto* megaPage = new QWidget(this);
    auto* megaLay  = new QVBoxLayout(megaPage);
    auto* megaLbl  = new QLabel("MEGA sync: export your DB to JSON and manually upload\n"
                                "via the MEGA web or desktop client.", megaPage);
    megaLbl->setWordWrap(true);
    megaLay->addWidget(megaLbl);
    megaLay->addStretch();
    m_stack->addWidget(megaPage);

    m_stack->addWidget(buildFtpPage());
    m_stack->addWidget(buildSmbPage());

    lay->addWidget(m_stack);

    // Status / progress
    m_statusLabel = new QLabel("Ready.", this);
    m_statusLabel->setStyleSheet("color:#8b949e; font-size:11px;");
    lay->addWidget(m_statusLabel);
    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setTextVisible(false);
    m_progress->setFixedHeight(4);
    m_progress->setStyleSheet(
        "QProgressBar { background:#21262d; border-radius:2px; }"
        "QProgressBar::chunk { background:#388bfd; border-radius:2px; }");
    lay->addWidget(m_progress);

    // Buttons
    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    m_syncBtn = new QPushButton("Sync Now", this);
    m_syncBtn->setObjectName("primaryButton");
    connect(m_syncBtn, &QPushButton::clicked, this, &CloudSyncDialog::onSyncNow);
    btnRow->addWidget(m_syncBtn);
    auto* closeBtn = new QPushButton("Close", this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnRow->addWidget(closeBtn);
    lay->addLayout(btnRow);

    connect(m_providerCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &CloudSyncDialog::onProviderChanged);
    connect(m_sync, &SyncManager::syncProgress,  this, &CloudSyncDialog::onSyncProgress);
    connect(m_sync, &SyncManager::syncFinished,  this, &CloudSyncDialog::onSyncFinished);
}

QWidget* CloudSyncDialog::buildOAuthPage(CloudProvider /*p*/) {
    auto* page = new QWidget(this);
    auto* lay  = new QVBoxLayout(page);

    auto* info = new QLabel(
        "1. Click 'Authenticate' to open the provider's login page in your browser.\n"
        "2. After authorizing, paste the access token below.\n"
        "3. Click 'Sync Now' to upload/download your database.", page);
    info->setWordWrap(true);
    lay->addWidget(info);

    auto* form = new QFormLayout;
    m_oauthToken = new QLineEdit(page);
    m_oauthToken->setPlaceholderText("Paste access token here…");
    form->addRow("Access Token:", m_oauthToken);
    lay->addLayout(form);

    auto* authBtn = new QPushButton("Authenticate →", page);
    connect(authBtn, &QPushButton::clicked, this, &CloudSyncDialog::onAuthenticate);
    lay->addWidget(authBtn);
    lay->addStretch();
    return page;
}

QWidget* CloudSyncDialog::buildFtpPage() {
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);
    m_ftpHost = new QLineEdit(page); m_ftpHost->setPlaceholderText("ftp.example.com");
    m_ftpPort = new QLineEdit(page); m_ftpPort->setPlaceholderText("21");
    m_ftpUser = new QLineEdit(page);
    m_ftpPass = new QLineEdit(page); m_ftpPass->setEchoMode(QLineEdit::Password);
    m_ftpPath = new QLineEdit(page); m_ftpPath->setPlaceholderText("/backups/snipQ_db.json");
    form->addRow("Host:",        m_ftpHost);
    form->addRow("Port:",        m_ftpPort);
    form->addRow("Username:",    m_ftpUser);
    form->addRow("Password:",    m_ftpPass);
    form->addRow("Remote path:", m_ftpPath);
    return page;
}

QWidget* CloudSyncDialog::buildSmbPage() {
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);
    m_smbHost = new QLineEdit(page); m_smbHost->setPlaceholderText("192.168.1.10");
    m_smbUser = new QLineEdit(page);
    m_smbPass = new QLineEdit(page); m_smbPass->setEchoMode(QLineEdit::Password);
    m_smbPath = new QLineEdit(page); m_smbPath->setPlaceholderText("share/backups");
    form->addRow("Host / IP:", m_smbHost);
    form->addRow("Username:",  m_smbUser);
    form->addRow("Password:",  m_smbPass);
    form->addRow("Share path:",m_smbPath);
    return page;
}

void CloudSyncDialog::onProviderChanged(int idx) {
    m_stack->setCurrentIndex(idx);
}

void CloudSyncDialog::onAuthenticate() {
    int idx = m_providerCombo->currentIndex();
    QUrl url;
    switch (idx) {
        case 0: url = "https://accounts.google.com/o/oauth2/auth?scope=https://www.googleapis.com/auth/drive.file"; break;
        case 1: url = "https://login.microsoftonline.com/common/oauth2/v2.0/authorize?scope=Files.ReadWrite"; break;
        case 2: url = "https://www.dropbox.com/oauth2/authorize"; break;
        case 3: url = "https://account.box.com/api/oauth2/authorize"; break;
        default: return;
    }
    QDesktopServices::openUrl(url);
}

void CloudSyncDialog::onSyncNow() {
    int idx = m_providerCombo->currentIndex();
    auto [name, provider] = PROVIDERS[idx];

    SyncConfig cfg;
    cfg.provider    = provider;
    cfg.localDbPath = m_db->currentPath();

    if (idx < 4) {
        cfg.accessToken = m_oauthToken ? m_oauthToken->text().trimmed() : QString();
    } else if (provider == CloudProvider::FTP) {
        cfg.host       = m_ftpHost->text().trimmed();
        cfg.port       = m_ftpPort->text().toInt();
        cfg.username   = m_ftpUser->text();
        cfg.password   = m_ftpPass->text();
        cfg.remotePath = m_ftpPath->text().trimmed();
    } else if (provider == CloudProvider::SMB) {
        cfg.host       = m_smbHost->text().trimmed();
        cfg.username   = m_smbUser->text();
        cfg.password   = m_smbPass->text();
        cfg.remotePath = m_smbPath->text().trimmed();
    }

    m_sync->configure(cfg);
    m_syncBtn->setEnabled(false);
    m_progress->setValue(0);
    m_statusLabel->setText("Starting sync…");
    m_sync->syncNow();
}

void CloudSyncDialog::onSyncProgress(int pct, const QString& msg) {
    m_progress->setValue(pct);
    m_statusLabel->setText(msg);
}

void CloudSyncDialog::onSyncFinished(bool ok, const QString& err) {
    m_syncBtn->setEnabled(true);
    m_progress->setValue(ok ? 100 : 0);
    m_statusLabel->setText(ok ? "✓ Sync complete." : QStringLiteral("✗ %1").arg(err));
}
