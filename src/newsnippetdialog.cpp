#include "newsnippetdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFormLayout>

static constexpr int NAME_MAX = 256;

NewSnippetDialog::NewSnippetDialog(const QList<Folder>& folders, int defaultFolder,
                                   QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("New Snippet");
    setFixedWidth(360);
    auto* lay = new QVBoxLayout(this);
    lay->setSpacing(12);
    lay->setContentsMargins(16,16,16,16);

    auto* form = new QFormLayout;

    // Name field with counter
    auto* nameRow = new QHBoxLayout;
    nameRow->setContentsMargins(0,0,0,0);
    nameRow->setSpacing(4);
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setMaxLength(NAME_MAX);
    m_nameEdit->setPlaceholderText("Untitled Snippet");
    nameRow->addWidget(m_nameEdit, 1);
    m_nameCounter = new QLabel(QStringLiteral("0/%1").arg(NAME_MAX), this);
    m_nameCounter->setStyleSheet("font-size:10px; color:#484f58;");
    nameRow->addWidget(m_nameCounter);
    form->addRow("Name:", nameRow);

    connect(m_nameEdit, &QLineEdit::textChanged, this, [this](const QString& t){
        int len = t.length();
        m_nameCounter->setText(QStringLiteral("%1/%2").arg(len).arg(NAME_MAX));
        bool over = (len >= NAME_MAX);
        m_nameCounter->setStyleSheet(
            over ? "font-size:10px; color:#f85149; font-weight:600;"
                 : "font-size:10px; color:#484f58;");
    });

    m_errorLabel = new QLabel(this);
    m_errorLabel->setStyleSheet("color: red;");
    m_errorLabel->hide();

    m_folderCombo = new QComboBox(this);
    m_folderCombo->addItem("(No folder)", -1);
    m_folderIds << -1;
    for (auto& f : folders) {
        m_folderCombo->addItem(f.name, f.id);
        m_folderIds << f.id;
        if (f.id == defaultFolder)
            m_folderCombo->setCurrentIndex(m_folderCombo->count() - 1);
    }
    form->addRow("Folder:", m_folderCombo);
    lay->addLayout(form);

    auto* row = new QHBoxLayout;
    row->addStretch();
    auto* ok     = new QPushButton("Create", this);
    ok->setObjectName("primaryButton");
    auto* cancel = new QPushButton("Cancel", this);
    connect(ok,     &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    row->addWidget(cancel);
    row->addWidget(ok);
    lay->addLayout(row);
    m_nameEdit->setFocus();
}

QString NewSnippetDialog::name() const    { return m_nameEdit->text().trimmed(); }
int     NewSnippetDialog::folderId() const {
    return m_folderCombo->currentData().toInt();
}
