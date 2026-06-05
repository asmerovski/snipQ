#include "settingsdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>
#include <QFile>
#include <QDir>

SettingsDialog::SettingsDialog(Database* db, QWidget* parent)
    : QDialog(parent), m_db(db) {
    setWindowTitle("Settings");
    setMinimumWidth(480);

    auto* lay = new QVBoxLayout(this);
    lay->setSpacing(16);
    lay->setContentsMargins(20, 20, 20, 20);

    // ── Storage section ──────────────────────────────────────
    auto* storageGroup = new QGroupBox("Storage Location", this);
    auto* sgLay = new QVBoxLayout(storageGroup);

    auto* hint = new QLabel("snipQ stores its database (SQLite) at this path.\n"
                            "Click 'Move' to relocate — data will be copied automatically.", this);
    hint->setWordWrap(true);
    hint->setStyleSheet("color:#6e7681; font-size:11px;");
    sgLay->addWidget(hint);

    auto* pathRow = new QHBoxLayout;
    m_storagePath = new QLineEdit(m_db->currentPath(), this);
    m_storagePath->setReadOnly(true);
    pathRow->addWidget(m_storagePath, 1);

    auto* browseBtn = new QPushButton("Browse…", this);
    connect(browseBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowseStorage);
    pathRow->addWidget(browseBtn);

    auto* moveBtn = new QPushButton("Move Here", this);
    moveBtn->setObjectName("primaryButton");
    connect(moveBtn, &QPushButton::clicked, this, &SettingsDialog::onMoveStorage);
    pathRow->addWidget(moveBtn);

    sgLay->addLayout(pathRow);
    lay->addWidget(storageGroup);

    // ── About ────────────────────────────────────────────────
    auto* aboutGroup = new QGroupBox("About", this);
    auto* abLay = new QVBoxLayout(aboutGroup);
    auto* about = new QLabel("snipQ v1.0.0\nA code snippet manager inspired by massCode.\n"
                             "Built with Qt6. Cross-platform: Windows, Linux, macOS.", this);
    about->setWordWrap(true);
    abLay->addWidget(about);
    lay->addWidget(aboutGroup);

    auto* closeBtn = new QPushButton("Close", this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    btnRow->addWidget(closeBtn);
    lay->addLayout(btnRow);
}

void SettingsDialog::onBrowseStorage() {
    QString dir = QFileDialog::getExistingDirectory(this, "Choose Storage Folder",
                                                    QDir(m_db->currentPath()).absolutePath());
    if (!dir.isEmpty()) {
        m_storagePath->setText(dir + "/snipQ.db");
    }
}

void SettingsDialog::onMoveStorage() {
    QString newPath = m_storagePath->text();
    if (newPath == m_db->currentPath()) return;

    // Ensure directory exists
    QDir().mkpath(QFileInfo(newPath).absolutePath());

    // Copy file
    if (QFile::exists(newPath)) {
        if (QMessageBox::question(this, "Overwrite?",
            "A database already exists at the target location. Overwrite?")
            != QMessageBox::Yes) return;
        QFile::remove(newPath);
    }

    if (!QFile::copy(m_db->currentPath(), newPath)) {
        QMessageBox::warning(this, "Error", "Could not copy the database file.");
        return;
    }

    emit storagePathChanged(newPath);
    QMessageBox::information(this, "Done",
        "Storage location updated. snipQ will use the new path on next start.");
}
