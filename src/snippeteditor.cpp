#include "snippeteditor.h"
#include "appsettings.h"
#include "codeformatter.h"
#include <QTimer>
#include <QApplication>
#include <QClipboard>
#include <QToolBar>
#include <QTextBlock>
#include <QDateTime>
#include <QFileInfo>

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
    m_titleEdit->setPlaceholderText("Snippet name\u2026");
    lay->addWidget(m_titleEdit);

    // ── Description ──────────────────────────────────────────
    m_descEdit = new QLineEdit(this);
    m_descEdit->setObjectName("SnippetDescription");
    m_descEdit->setPlaceholderText("Short description (optional)\u2026");
    m_descEdit->setStyleSheet(
        "QLineEdit#SnippetDescription {"
        "  background: transparent;"
        "  border: none;"
        "  border-bottom: 1px solid #21262d;"
        "  padding: 5px 16px;"
        "  font-size: 12px;"
        "  color: #8b949e;"
        "}"
        "QLineEdit#SnippetDescription:focus {"
        "  border-bottom-color: #388bfd;"
        "  color: #c9d1d9;"
        "}");
    lay->addWidget(m_descEdit);

    // ── Timestamps ───────────────────────────────────────────
    m_timestampBar = new QLabel(this);
    m_timestampBar->setObjectName("TimestampBar");
    m_timestampBar->setStyleSheet(
        "QLabel#TimestampBar {"
        "  background: transparent;"
        "  color: #484f58;"
        "  font-size: 10px;"
        "  padding: 3px 16px 3px 16px;"
        "  border-bottom: 1px solid #21262d;"
        "}");
    m_timestampBar->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_timestampBar->setTextFormat(Qt::PlainText);
    lay->addWidget(m_timestampBar);

    // ── Toolbar ──────────────────────────────────────────────
    auto* toolbar = new QToolBar(this);
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(14, 14));

    m_langCombo = new QComboBox(this);
    m_langCombo->addItems(LANGUAGES);
    m_langCombo->setMinimumWidth(130);
    toolbar->addWidget(m_langCombo);

    toolbar->addSeparator();

    auto* fmtBtn = new QToolButton(this);
    fmtBtn->setText("\u2261  Format");
    fmtBtn->setToolTip("Format / align code  (Ctrl+Shift+F)");
    fmtBtn->setShortcut(QKeySequence("Ctrl+Shift+F"));
    toolbar->addWidget(fmtBtn);

    toolbar->addSeparator();

    auto* copyBtn = new QToolButton(this);
    copyBtn->setText("\u29c9  Copy");
    copyBtn->setToolTip("Copy content to clipboard  (Ctrl+Shift+C)");
    copyBtn->setShortcut(QKeySequence("Ctrl+Shift+C"));
    toolbar->addWidget(copyBtn);

    toolbar->addSeparator();

    // Keyboard shortcut hint label
    auto* hintLabel = new QLabel(
        "Ctrl+/: comment  \u2502  Tab: indent  \u2502  Shift+Tab: unindent", this);
    hintLabel->setStyleSheet("color:#484f58; font-size:10px; padding:0 8px;");
    toolbar->addWidget(hintLabel);

    auto* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(spacer);

    m_charCount = new QLabel("0 chars", this);
    m_charCount->setStyleSheet("color:#484f58; font-size:11px; padding:0 8px;");
    toolbar->addWidget(m_charCount);

    lay->addWidget(toolbar);

    // ── Code editor (with line numbers + smart keys) ─────────
    m_editor = new CodeEditor(this);
    m_editor->setPlaceholderText("// Start typing your snippet\u2026");
    m_editor->setFont(AppSettings::instance().editorFont());
    m_editor->setTabWidth(4);
    m_editor->viewport()->setCursor(Qt::IBeamCursor);

    // Syntax highlighter — owned by the document
    m_highlighter = new Highlighter(m_editor->document());

    // Live font updates from Settings dialog
    // (CodeEditor already connects internally; this keeps charCount updated)
    connect(&AppSettings::instance(), &AppSettings::fontChanged,
            this, [this](const QFont&) {
                if (m_highlighter) m_highlighter->rehighlight();
            });

    lay->addWidget(m_editor, 1);

    // ── Tags bar ─────────────────────────────────────────────
    buildTagsBar();
    lay->addWidget(m_tagsBar);

    // ── Auto-save timer ───────────────────────────────────────
    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(600);

    // ── Wire signals ──────────────────────────────────────────
    connect(m_saveTimer,  &QTimer::timeout,
            this,         &SnippetEditor::autoSave);
    connect(m_titleEdit,  &QLineEdit::textChanged,
            this,         &SnippetEditor::onTitleChanged);
    connect(m_descEdit,   &QLineEdit::textChanged,
            this,         &SnippetEditor::onDescriptionChanged);
    connect(m_editor,     &QPlainTextEdit::textChanged,
            this,         &SnippetEditor::onContentChanged);
    connect(m_langCombo,  &QComboBox::currentTextChanged,
            this,         &SnippetEditor::onLanguageChanged);
    connect(fmtBtn,       &QToolButton::clicked,
            this,         &SnippetEditor::onFormatCode);
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
    m_descEdit->clear();
    m_descEdit->setEnabled(false);
    m_timestampBar->clear();
    m_editor->clear();
    m_editor->setEnabled(false);
    m_langCombo->setEnabled(false);
    m_tagInput->clear();
    rebuildTagChips();
    m_charCount->setText(QStringLiteral("\u2014"));
    if (m_highlighter) m_highlighter->setLanguage("plaintext");
    m_dirty   = false;
    m_loading = false;
}

void SnippetEditor::loadSnippet(int id)
{
    m_saveTimer->stop();

    // Flush previous snippet silently (block dataChanged to avoid re-entrant refresh)
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
    m_descEdit->setEnabled(true);
    m_editor->setEnabled(true);
    m_langCombo->setEnabled(true);

    // Block all widget signals while populating
    {
        QSignalBlocker tb(m_titleEdit);
        QSignalBlocker eb(m_editor);
        QSignalBlocker lb(m_langCombo);

        m_titleEdit->setText(m_snippet.name);
        m_descEdit->setText(m_snippet.description);
        m_editor->setPlainText(m_snippet.content);

        // Move cursor to top without triggering scroll-jump
        QTextCursor tc = m_editor->textCursor();
        tc.movePosition(QTextCursor::Start);
        m_editor->setTextCursor(tc);

        int idx = LANGUAGES.indexOf(m_snippet.language);
        m_langCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    }

    applyLanguage(m_snippet.language);

    // Format timestamps — show local time, human-friendly
    auto fmtDt = [](const QDateTime& dt) -> QString {
        if (!dt.isValid()) return QStringLiteral("unknown");
        QDateTime local = dt.toLocalTime();
        // If within last 7 days show relative, otherwise full date
        qint64 secsAgo = local.secsTo(QDateTime::currentDateTime());
        if (secsAgo < 60)
            return QStringLiteral("just now");
        if (secsAgo < 3600)
            return QStringLiteral("%1 min ago").arg(secsAgo / 60);
        if (secsAgo < 86400)
            return QStringLiteral("%1 hr ago").arg(secsAgo / 3600);
        if (secsAgo < 7 * 86400)
            return QStringLiteral("%1 days ago").arg(secsAgo / 86400);
        return local.toString("d MMM yyyy  hh:mm");
    };

    m_timestampBar->setText(
        QStringLiteral("Created: %1   \u2502   Modified: %2")
        .arg(fmtDt(m_snippet.createdAt),
             fmtDt(m_snippet.updatedAt)));
    rebuildTagChips();
    m_charCount->setText(
        QStringLiteral("%1 chars").arg(m_snippet.content.length()));

    m_dirty   = false;
    m_loading = false;
}

// ─── Private helpers ─────────────────────────────────────────────────────────

void SnippetEditor::applyLanguage(const QString& lang)
{
    m_editor->setLanguage(lang);           // sets comment prefix for Ctrl+/
    if (m_highlighter)
        m_highlighter->setLanguage(lang);  // recolours the document
}

// ─── Slots ───────────────────────────────────────────────────────────────────

void SnippetEditor::onTitleChanged(const QString& text)
{
    if (m_loading || !m_snippet.isValid()) return;
    m_snippet.name = text;
    scheduleAutoSave();
}

void SnippetEditor::onDescriptionChanged(const QString& text)
{
    if (m_loading || !m_snippet.isValid()) return;
    m_snippet.description = text;
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
    applyLanguage(lang);
    scheduleAutoSave();
}

void SnippetEditor::onFormatCode()
{
    if (!m_snippet.isValid() || !m_editor->isEnabled()) return;

    QString original  = m_editor->toPlainText();
    if (original.trimmed().isEmpty()) return;

    QString formatted = CodeFormatter::format(original, m_snippet.language);
    if (formatted == original) return;

    // Preserve approximate cursor position by line number
    int cursorLine = m_editor->textCursor().blockNumber();

    QTextCursor tc = m_editor->textCursor();
    tc.beginEditBlock();
    tc.select(QTextCursor::Document);
    tc.insertText(formatted);
    tc.endEditBlock();

    // Restore cursor line
    QTextBlock blk = m_editor->document()->findBlockByNumber(
        qMin(cursorLine, m_editor->document()->blockCount() - 1));
    QTextCursor nc(blk);
    m_editor->setTextCursor(nc);
    m_editor->ensureCursorVisible();
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
    m_snippet.content     = m_editor->toPlainText();
    m_snippet.description = m_descEdit->text().trimmed();
    m_db->updateSnippet(m_snippet);
    m_dirty = false;
    emit snippetModified(m_snippet.id);
}

void SnippetEditor::buildTagsBar()
{
    m_tagsBar = new QWidget(this);
    m_tagsBar->setObjectName("TagsBar");
    m_tagsLayout = new QHBoxLayout(m_tagsBar);
    m_tagsLayout->setContentsMargins(8, 6, 8, 6);
    m_tagsLayout->setSpacing(6);

    m_tagInput = new QLineEdit(m_tagsBar);
    m_tagInput->setPlaceholderText("Add tag\u2026");
    m_tagInput->setFixedWidth(110);
    m_tagInput->setStyleSheet(
        "background:#161b22; border:1px solid #30363d; border-radius:4px;"
        "padding:2px 8px; font-size:11px; color:#8b949e;");
    connect(m_tagInput, &QLineEdit::returnPressed,
            this, &SnippetEditor::onAddTag);

    m_tagsLayout->addWidget(m_tagInput);
    m_tagsLayout->addStretch();
}

void SnippetEditor::rebuildTagChips()
{
    // Remove all chip widgets (keep m_tagInput + stretch)
    QList<QWidget*> chips;
    for (int i = 0; i < m_tagsLayout->count(); ++i) {
        auto* item = m_tagsLayout->itemAt(i);
        if (!item) continue;
        auto* w = item->widget();
        if (w && w != m_tagInput) chips << w;
    }
    for (auto* w : chips) { m_tagsLayout->removeWidget(w); delete w; }

    for (const QString& tag : m_snippet.tags) {
        auto* chip = new QToolButton(m_tagsBar);
        chip->setText(QStringLiteral("# ") + tag + QStringLiteral("  \u00d7"));
        chip->setStyleSheet(
            "QToolButton{background:#1f3352;color:#58a6ff;"
            "border:1px solid #2d4a6e;border-radius:4px;"
            "font-size:11px;padding:2px 6px;}"
            "QToolButton:hover{background:#2d4a6e;}");
        QString tagCopy = tag;
        connect(chip, &QToolButton::clicked, this, [this, tagCopy]() {
            m_snippet.tags.removeAll(tagCopy);
            rebuildTagChips();
            scheduleAutoSave();
        });
        m_tagsLayout->insertWidget(m_tagsLayout->count() - 1, chip);
    }
}

void SnippetEditor::scheduleAutoSave()
{
    m_dirty = true;
    m_saveTimer->start();
}
