#include "newfolderdialog.h"
#include "limits.h"
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

    // Name row: input + live counter
    auto* nameRow = new QHBoxLayout;
    nameRow->setContentsMargins(0, 0, 0, 0);
    nameRow->setSpacing(0);
    m_nameEdit = new QLineEdit(initial, this);
    m_nameEdit->setMaxLength(NAME_MAX_LEN);
    m_nameEdit->setPlaceholderText("My Snippets");
    m_nameEdit->selectAll();
    nameRow->addWidget(m_nameEdit, 1);

    m_counter = new QLabel(QStringLiteral("%1/%2").arg(initial.length()).arg(NAME_MAX_LEN), this);
    m_counter->setStyleSheet("font-size:10px; color:#484f58; padding:0 6px;");
    m_counter->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    nameRow->addWidget(m_counter);
    lay->addLayout(nameRow);

    connect(m_nameEdit, &QLineEdit::textChanged, this, [this](const QString& t) {
        int len = t.length();
        m_counter->setText(QStringLiteral("%1/%2").arg(len).arg(NAME_MAX_LEN));
        m_counter->setStyleSheet(len >= NAME_MAX_LEN
            ? "font-size:10px; color:#f85149; font-weight:600; padding:0 6px;"
            : "font-size:10px; color:#484f58; padding:0 6px;");
    });

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
