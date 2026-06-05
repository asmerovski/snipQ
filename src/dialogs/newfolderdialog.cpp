#include "newfolderdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>

NewFolderDialog::NewFolderDialog(const QString& title, const QString& initial, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(title);
    setFixedWidth(320);
    auto* lay = new QVBoxLayout(this);
    lay->setSpacing(12);
    lay->setContentsMargins(16,16,16,16);
    lay->addWidget(new QLabel("Folder name:", this));
    m_nameEdit = new QLineEdit(initial, this);
    m_nameEdit->setPlaceholderText("My Snippets");
    m_nameEdit->selectAll();
    lay->addWidget(m_nameEdit);
    auto* row = new QHBoxLayout;
    row->addStretch();
    auto* ok = new QPushButton("OK", this);
    ok->setObjectName("primaryButton");
    auto* cancel = new QPushButton("Cancel", this);
    connect(ok,     &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    row->addWidget(cancel);
    row->addWidget(ok);
    lay->addLayout(row);
    m_nameEdit->setFocus();
}

QString NewFolderDialog::folderName() const { return m_nameEdit->text().trimmed(); }
