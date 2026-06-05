#include "newsnippetdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFormLayout>

NewSnippetDialog::NewSnippetDialog(const QList<Folder>& folders, int defaultFolder,
                                   QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("New Snippet");
    setFixedWidth(360);
    auto* lay = new QVBoxLayout(this);
    lay->setSpacing(12);
    lay->setContentsMargins(16,16,16,16);

    auto* form = new QFormLayout;
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText("Untitled Snippet");
    form->addRow("Name:", m_nameEdit);

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
