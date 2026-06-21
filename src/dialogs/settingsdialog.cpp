#include "settingsdialog.h"
#include "appsettings.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QDir>
#include <QFontDatabase>

SettingsDialog::SettingsDialog(Database* db, QWidget* parent)
    : QDialog(parent), m_db(db)
{
    setWindowTitle("Settings");
    setMinimumWidth(500);

    auto* lay = new QVBoxLayout(this);
    lay->setSpacing(16);
    lay->setContentsMargins(20, 20, 20, 20);

    // ── Editor Font ──────────────────────────────────────────
    auto* fontGroup = new QGroupBox("Editor Font", this);
    auto* fgLay = new QVBoxLayout(fontGroup);

    auto* fontForm = new QFormLayout;
    fontForm->setSpacing(10);

    // QFontComboBox filtered to monospace only
    m_fontCombo = new QFontComboBox(this);
    m_fontCombo->setFontFilters(QFontComboBox::MonospacedFonts);
    m_fontCombo->setCurrentFont(AppSettings::instance().editorFont());
    fontForm->addRow("Family:", m_fontCombo);

    m_fontSizeSpin = new QSpinBox(this);
    m_fontSizeSpin->setRange(8, 32);
    m_fontSizeSpin->setSuffix(" pt");
    m_fontSizeSpin->setValue(AppSettings::instance().editorFont().pointSize());
    fontForm->addRow("Size:", m_fontSizeSpin);

    fgLay->addLayout(fontForm);

    // Live preview
    m_fontPreview = new QLabel(
        "fn main() {\n    println!(\"Hello, snipQ!\");\n}", this);
    m_fontPreview->setFont(AppSettings::instance().editorFont());
    m_fontPreview->setStyleSheet(
        "background:#0d1117; color:#c9d1d9; border:1px solid #30363d;"
        "border-radius:6px; padding:10px 14px;");
    m_fontPreview->setFixedHeight(72);
    fgLay->addWidget(m_fontPreview);

    lay->addWidget(fontGroup);

    // ── Storage ───────────────────────────────────────────────
    auto* storageGroup = new QGroupBox("Storage Location", this);
    auto* sgLay = new QVBoxLayout(storageGroup);

    auto* hint = new QLabel(
        "snipQ stores its database (SQLite) at this path.\n"
        "Click 'Move' to relocate — data will be copied automatically.", this);
    hint->setWordWrap(true);
    hint->setStyleSheet("color:#6e7681; font-size:11px;");
    sgLay->addWidget(hint);

    auto* pathRow = new QHBoxLayout;
    m_storagePath = new QLineEdit(m_db->currentPath(), this);
    m_storagePath->setReadOnly(true);
    pathRow->addWidget(m_storagePath, 1);

    auto* browseBtn = new QPushButton("Browse\u2026", this);
    connect(browseBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowseStorage);
    pathRow->addWidget(browseBtn);

    auto* moveBtn = new QPushButton("Move Here", this);
    moveBtn->setObjectName("primaryButton");
    connect(moveBtn, &QPushButton::clicked, this, &SettingsDialog::onMoveStorage);
    pathRow->addWidget(moveBtn);

    sgLay->addLayout(pathRow);
    lay->addWidget(storageGroup);


    // ── Buttons ───────────────────────────────────────────────
    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    auto* closeBtn = new QPushButton("Close", this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnRow->addWidget(closeBtn);
    lay->addLayout(btnRow);

    // ── Connections ───────────────────────────────────────────
    connect(m_fontCombo,    &QFontComboBox::currentFontChanged,
            this,           &SettingsDialog::onFontFamilyChanged);
    connect(m_fontSizeSpin, qOverload<int>(&QSpinBox::valueChanged),
            this,           &SettingsDialog::onFontSizeChanged);
}

void SettingsDialog::onFontFamilyChanged(const QFont& font)
{
    QFont f = AppSettings::makeMono(font.family(),
                                    m_fontSizeSpin->value());
    m_fontPreview->setFont(f);
    AppSettings::instance().setEditorFont(f);
}

void SettingsDialog::onFontSizeChanged(int size)
{
    QFont f = AppSettings::makeMono(m_fontCombo->currentFont().family(), size);
    m_fontPreview->setFont(f);
    AppSettings::instance().setEditorFont(f);
}

void SettingsDialog::onBrowseStorage()
{
    QString dir = QFileDialog::getExistingDirectory(
        this, "Choose Storage Folder",
        QFileInfo(m_db->currentPath()).absolutePath());
    if (!dir.isEmpty())
        m_storagePath->setText(dir + "/snipQ.db");
}

void SettingsDialog::onMoveStorage()
{
    QString newPath = m_storagePath->text();
    if (newPath == m_db->currentPath()) return;

    QDir().mkpath(QFileInfo(newPath).absolutePath());

    if (QFile::exists(newPath)) {
        if (QMessageBox::question(this, "Overwrite?",
            "A database already exists at the target. Overwrite?")
            != QMessageBox::Yes) return;
        QFile::remove(newPath);
    }

    if (!QFile::copy(m_db->currentPath(), newPath)) {
        QMessageBox::warning(this, "Error", "Could not copy the database file.");
        return;
    }

    emit storagePathChanged(newPath);
    QMessageBox::information(this, "Done",
        "Storage updated. Restart snipQ to use the new path.");
}
