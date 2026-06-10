#include "snippeteditor.h"
#include "appsettings.h"
#include <QTimer>
#include <QApplication>
#include <QClipboard>
#include <QFontInfo>
#include <QToolBar>
#include <QScrollBar>

static const QStringList LANGUAGES = {
    "plaintext","bash","c","cpp","csharp","css","dart","diff","dockerfile",
    "go","graphql","html","http","java","javascript","json","kotlin","latex",
    "lua","makefile","markdown","nginx","objectivec","php","powershell",
    "python","r","ruby","rust","scala","shell","sql","swift","toml",
    "typescript","xml","yaml"
};

// ─────────────────────────────────────────────────────────────────────────────

SnippetEditor::SnippetEditor(Database* db, QWidget* parent)
    : QWidget(parent), m_db(db)
{
    setObjectName("EditorPanel");
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    // ── Title ────────────────────────────────────────────────
    m_titleEdit = new QLineEdit(this);
    m_titleEdit->setObjectName("SnippetTitle");
    m_titleEdit->setPlaceholderText("Snippet name…");
    lay->addWidget(m_titleEdit);

    // ── Toolbar ──────────────────────────────────────────────
    auto* toolbar = new QToolBar(this);
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(14, 14));

    m_langCombo = new QComboBox(this);
    m_langCombo->addItems(LANGUAGES);
    m_langCombo->setMinimumWidth(130);
    toolbar->addWidget(m_langCombo);

    toolbar->addSeparator();

    auto* copyBtn = new QToolButton(this);
    copyBtn->setText("\u29c9  Copy");
    copyBtn->setToolTip("Copy content to clipboard");
    toolbar->addWidget(copyBtn);

    toolbar->addSeparator();

    m_charCount = new QLabel("0 chars", this);
    m_charCount->setStyleSheet("color:#484f58; font-size:11px; padding:0 8px;");
    toolbar->addWidget(m_charCount);

    lay->addWidget(toolbar);

    // ── Code editor ──────────────────────────────────────────
    m_editor = new QPlainTextEdit(this);
    m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_editor->setPlaceholderText("// Start typing your snippet…");
    m_editor->setFont(AppSettings::instance().editorFont());
    m_editor->viewport()->setCursor(Qt::IBeamCursor);
    lay->addWidget(m_editor, 1);

    // Live font updates from Settings dialog
    connect(&AppSettings::instance(), &AppSettings::fontChanged,
            this, [this](const QFont& f) {
                m_editor->setFont(f);
            });

    // ── Tags bar ─────────────────────────────────────────────
    buildTagsBar();
    lay->addWidget(m_tagsBar);

    // ── Auto-save timer ───────────────────────────────────────
    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(600);

    // ── Connections ──────────────────────────────────────────
    connect(m_saveTimer,  &QTimer::timeout,
            this,         &SnippetEditor::autoSave);
    connect(m_titleEdit,  &QLineEdit::textChanged,
            this,         &SnippetEditor::onTitleChanged);
    connect(m_editor,     &QPlainTextEdit::textChanged,
            this,         &SnippetEditor::onContentChanged);
    connect(m_langCombo,  &QComboBox::currentTextChanged,
            this,         &SnippetEditor::onLanguageChanged);
    connect(copyBtn,      &QToolButton::clicked,
            this,         &SnippetEditor::onCopyContent);

    clearEditor();
}

// ─── Public ──────────────────────────────────────────────────────────────────

void SnippetEditor::clearEditor()
{
    m_loading = true;
    m_snippet = Snippet{};
    m_titleEdit->clear();
    m_titleEdit->setEnabled(false);
    m_editor->clear();
    m_editor->setEnabled(false);
    m_langCombo->setEnabled(false);
    m_tagInput->clear();
    rebuildTagChips();
    m_charCount->setText(QStringLiteral("\u2014"));
    m_dirty   = false;
    m_loading = false;
}

void SnippetEditor::loadSnippet(int id)
{
    // Stop timer first so it can't fire mid-load
    m_saveTimer->stop();

    // Flush dirty state for the PREVIOUS snippet, blocking dataChanged so the
    // list doesn't rebuild re-entrantly while we're inside onCurrentRowChanged
    if (m_dirty && m_snippet.isValid()) {
        QSignalBlocker dbBlocker(m_db);
        autoSave();
    }
    m_dirty   = false;
    m_loading = true;

    m_snippet = m_db->snippetById(id);
    if (!m_snippet.isValid()) {
        clearEditor();
        return;
    }

    m_titleEdit->setEnabled(true);
    m_editor->setEnabled(true);
    m_langCombo->setEnabled(true);

    // Block widget signals while we populate to avoid spurious dirty-marks
    {
        QSignalBlocker tb(m_titleEdit);
        QSignalBlocker eb(m_editor);
        QSignalBlocker lb(m_langCombo);

        m_titleEdit->setText(m_snippet.name);

        m_editor->setPlainText(m_snippet.content);
        // Restore cursor to top without triggering a scroll-to-cursor jump
        QTextCursor tc = m_editor->textCursor();
        tc.movePosition(QTextCursor::Start);
        m_editor->setTextCursor(tc);

        int idx = LANGUAGES.indexOf(m_snippet.language);
        m_langCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    }

    rebuildTagChips();
    m_charCount->setText(
        QStringLiteral("%1 chars").arg(m_snippet.content.length()));

    m_dirty   = false;
    m_loading = false;
}

// ─── Private slots ───────────────────────────────────────────────────────────

void SnippetEditor::onTitleChanged(const QString& text)
{
    if (m_loading || !m_snippet.isValid()) return;
    m_snippet.name = text;
    scheduleAutoSave();
}

void SnippetEditor::onContentChanged()
{
    if (m_loading || !m_snippet.isValid()) return;
    int len = m_editor->toPlainText().length();
    m_charCount->setText(QStringLiteral("%1 chars").arg(len));
    scheduleAutoSave();
}

void SnippetEditor::onLanguageChanged(const QString& lang)
{
    if (m_loading || !m_snippet.isValid()) return;
    m_snippet.language = lang;
    scheduleAutoSave();
}

void SnippetEditor::onCopyContent()
{
    QApplication::clipboard()->setText(m_editor->toPlainText());
}

void SnippetEditor::onAddTag()
{
    QString tag = m_tagInput->text().trimmed().toLower();
    if (tag.isEmpty() || !m_snippet.isValid()) return;
    if (!m_snippet.tags.contains(tag)) {
        m_snippet.tags.append(tag);
        m_tagInput->clear();
        rebuildTagChips();
        scheduleAutoSave();
    }
}

void SnippetEditor::autoSave()
{
    if (!m_dirty || !m_snippet.isValid()) return;
    m_snippet.content = m_editor->toPlainText();
    m_db->updateSnippet(m_snippet);
    m_dirty = false;
    emit snippetModified(m_snippet.id);
}

// ─── Private helpers ─────────────────────────────────────────────────────────

void SnippetEditor::buildTagsBar()
{
    m_tagsBar = new QWidget(this);
    m_tagsBar->setObjectName("TagsBar");
    m_tagsLayout = new QHBoxLayout(m_tagsBar);
    m_tagsLayout->setContentsMargins(8, 6, 8, 6);
    m_tagsLayout->setSpacing(6);

    m_tagInput = new QLineEdit(m_tagsBar);
    m_tagInput->setPlaceholderText("Add tag…");
    m_tagInput->setFixedWidth(110);
    m_tagInput->setStyleSheet(
        "background:#161b22; border:1px solid #30363d; border-radius:4px;"
        "padding:2px 8px; font-size:11px; color:#8b949e;");
    connect(m_tagInput, &QLineEdit::returnPressed,
            this,       &SnippetEditor::onAddTag);

    m_tagsLayout->addWidget(m_tagInput);
    m_tagsLayout->addStretch();
}

void SnippetEditor::rebuildTagChips()
{
    // Remove all chip widgets (leave m_tagInput and the stretch)
    QList<QWidget*> chips;
    for (int i = 0; i < m_tagsLayout->count(); ++i) {
        auto* item = m_tagsLayout->itemAt(i);
        if (!item) continue;
        auto* w = item->widget();
        if (w && w != m_tagInput) chips << w;
    }
    for (auto* w : chips) {
        m_tagsLayout->removeWidget(w);
        delete w;
    }

    for (const QString& tag : m_snippet.tags) {
        auto* chip = new QToolButton(m_tagsBar);
        chip->setText(QStringLiteral("# ") + tag + QStringLiteral("  \u00d7"));
        chip->setStyleSheet(
            "QToolButton { background:#1f3352; color:#58a6ff;"
            "border:1px solid #2d4a6e; border-radius:4px;"
            "font-size:11px; padding:2px 6px; }"
            "QToolButton:hover { background:#2d4a6e; }");
        QString tagCopy = tag;
        connect(chip, &QToolButton::clicked, this, [this, tagCopy]() {
            m_snippet.tags.removeAll(tagCopy);
            rebuildTagChips();
            scheduleAutoSave();
        });
        // Insert before the stretch (last item)
        m_tagsLayout->insertWidget(m_tagsLayout->count() - 1, chip);
    }
}

void SnippetEditor::scheduleAutoSave()
{
    m_dirty = true;
    m_saveTimer->start();
}
