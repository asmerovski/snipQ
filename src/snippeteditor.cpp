#include "snippeteditor.h"
#include "appsettings.h"
#include "codeformatter.h"
#include <QTimer>
#include <QApplication>
#include <QClipboard>
#include <QFontInfo>
#include <QToolBar>
#include <QScrollBar>
#include <QTextBlock>

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

    m_charCount = new QLabel("0 chars", this);
    m_charCount->setStyleSheet("color:#484f58; font-size:11px; padding:0 8px;");
    toolbar->addWidget(m_charCount);

    lay->addWidget(toolbar);

    // ── Code editor ──────────────────────────────────────────
    m_editor = new QPlainTextEdit(this);
    m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_editor->setPlaceholderText("// Start typing your snippet\u2026");
    m_editor->setFont(AppSettings::instance().editorFont());
    m_editor->viewport()->setCursor(Qt::IBeamCursor);

    // Tab = 4 spaces visually
    QFontMetrics fm(m_editor->font());
    m_editor->setTabStopDistance(fm.horizontalAdvance(' ') * 4);

    lay->addWidget(m_editor, 1);

    // Syntax highlighter — attached to the document, lives until re-assigned
    m_highlighter = new Highlighter(m_editor->document());

    // Live font updates from Settings
    connect(&AppSettings::instance(), &AppSettings::fontChanged,
            this, [this](const QFont& f) {
                m_editor->setFont(f);
                QFontMetrics fm2(f);
                m_editor->setTabStopDistance(fm2.horizontalAdvance(' ') * 4);
                // Rehighlight after font change (char format metrics may shift)
                if (m_highlighter) m_highlighter->rehighlight();
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

    {
        QSignalBlocker tb(m_titleEdit);
        QSignalBlocker eb(m_editor);
        QSignalBlocker lb(m_langCombo);

        m_titleEdit->setText(m_snippet.name);
        m_editor->setPlainText(m_snippet.content);

        QTextCursor tc = m_editor->textCursor();
        tc.movePosition(QTextCursor::Start);
        m_editor->setTextCursor(tc);

        int idx = LANGUAGES.indexOf(m_snippet.language);
        m_langCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    }

    // Apply highlighter for this language
    applyHighlighter(m_snippet.language);

    rebuildTagChips();
    m_charCount->setText(
        QStringLiteral("%1 chars").arg(m_snippet.content.length()));

    m_dirty   = false;
    m_loading = false;
}

// ─── Private helpers ─────────────────────────────────────────────────────────

void SnippetEditor::applyHighlighter(const QString& lang)
{
    if (!m_highlighter) {
        m_highlighter = new Highlighter(m_editor->document());
    }
    m_highlighter->setLanguage(lang);
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
    applyHighlighter(lang);
    scheduleAutoSave();
}

void SnippetEditor::onFormatCode()
{
    if (!m_snippet.isValid() || !m_editor->isEnabled()) return;

    QString original = m_editor->toPlainText();
    if (original.trimmed().isEmpty()) return;

    QString formatted = CodeFormatter::format(original, m_snippet.language);
    if (formatted == original) return;  // nothing changed

    // Preserve cursor line roughly
    int cursorLine = m_editor->textCursor().blockNumber();

    // Replace content with undo support
    QTextCursor tc = m_editor->textCursor();
    tc.beginEditBlock();
    tc.select(QTextCursor::Document);
    tc.insertText(formatted);
    tc.endEditBlock();

    // Restore cursor to same approximate line
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
    m_snippet.content = m_editor->toPlainText();
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
        m_tagsLayout->insertWidget(m_tagsLayout->count() - 1, chip);
    }
}

void SnippetEditor::scheduleAutoSave()
{
    m_dirty = true;
    m_saveTimer->start();
}
