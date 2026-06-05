#include "snippeteditor.h"
#include <QTimer>
#include <QApplication>
#include <QClipboard>
#include <QScrollArea>
#include <QToolBar>

static const QStringList LANGUAGES = {
    "plaintext","bash","c","cpp","csharp","css","dart","diff","dockerfile",
    "go","graphql","html","http","java","javascript","json","kotlin","latex",
    "lua","makefile","markdown","nginx","objectivec","php","powershell",
    "python","r","ruby","rust","scala","shell","sql","swift","toml",
    "typescript","xml","yaml"
};

SnippetEditor::SnippetEditor(Database* db, QWidget* parent)
    : QWidget(parent), m_db(db) {
    setObjectName("EditorPanel");
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    // ── Title bar ───────────────────────────────────────────
    m_titleEdit = new QLineEdit(this);
    m_titleEdit->setObjectName("SnippetTitle");
    m_titleEdit->setPlaceholderText("Snippet name…");
    lay->addWidget(m_titleEdit);

    // ── Toolbar (lang selector + actions) ───────────────────
    auto* toolbar = new QToolBar(this);
    toolbar->setMovable(false);

    m_langCombo = new QComboBox(this);
    m_langCombo->addItems(LANGUAGES);
    m_langCombo->setMinimumWidth(130);
    toolbar->addWidget(m_langCombo);

    toolbar->addSeparator();

    auto* copyBtn = new QToolButton(this);
    copyBtn->setText("⧉ Copy");
    copyBtn->setToolTip("Copy current tab content");
    toolbar->addWidget(copyBtn);

    toolbar->addSeparator();

    auto* addTabBtn = new QToolButton(this);
    addTabBtn->setText("＋ Tab");
    addTabBtn->setToolTip("Add fragment tab");
    toolbar->addWidget(addTabBtn);

    toolbar->addSeparator();

    m_charCount = new QLabel("0 chars", this);
    m_charCount->setStyleSheet("color:#484f58; font-size:11px; padding:0 8px;");
    toolbar->addWidget(m_charCount);

    lay->addWidget(toolbar);

    // ── Code tabs ────────────────────────────────────────────
    m_tabs = new QTabWidget(this);
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    lay->addWidget(m_tabs);

    // ── Tags bar ─────────────────────────────────────────────
    buildTagsBar();
    lay->addWidget(m_tagsBar);

    // ── Timer for auto-save ──────────────────────────────────
    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(800);
    connect(m_saveTimer, &QTimer::timeout, this, &SnippetEditor::autoSave);

    // ── Connections ──────────────────────────────────────────
    connect(m_titleEdit, &QLineEdit::textChanged, this, &SnippetEditor::onTitleChanged);
    connect(m_langCombo, &QComboBox::currentTextChanged, this, &SnippetEditor::onLanguageChanged);
    connect(addTabBtn,   &QToolButton::clicked, this, &SnippetEditor::onAddTab);
    connect(copyBtn,     &QToolButton::clicked, this, &SnippetEditor::onCopyContent);
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &SnippetEditor::onCloseTab);

    clearEditor();
}

void SnippetEditor::clearEditor() {
    m_loading = true;
    m_snippet = Snippet{};
    m_titleEdit->clear();
    m_titleEdit->setEnabled(false);
    m_tabs->clear();
    m_langCombo->setEnabled(false);
    m_tagInput->clear();
    rebuildTagChips();
    m_charCount->setText("—");
    m_loading = false;
}

void SnippetEditor::loadSnippet(int id) {
    autoSave(); // flush any pending save
    m_loading = true;
    m_snippet = m_db->snippetById(id);
    if (!m_snippet.isValid()) {
        clearEditor();
        return;
    }

    m_titleEdit->setEnabled(true);
    m_langCombo->setEnabled(true);
    m_titleEdit->setText(m_snippet.name);

    m_tabs->clear();
    for (auto& tab : m_snippet.tabs) {
        auto* editor = new QPlainTextEdit(this);
        editor->setPlainText(tab.content);
        editor->setLineWrapMode(QPlainTextEdit::NoWrap);
        editor->document()->setDefaultFont(QFont("JetBrains Mono,Cascadia Code,Fira Code,Consolas", 13));
        m_tabs->addTab(editor, tab.name);
        connect(editor, &QPlainTextEdit::textChanged, this, &SnippetEditor::onTabContentChanged);
    }
    if (m_tabs->count() > 0) {
        m_tabs->setCurrentIndex(0);
        // Set language combo
        QString lang = m_snippet.tabs[0].language;
        int idx = LANGUAGES.indexOf(lang);
        m_langCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    }

    rebuildTagChips();
    m_dirty = false;
    m_loading = false;
    onTabContentChanged(); // update char count
}

void SnippetEditor::onTitleChanged(const QString& text) {
    if (m_loading || !m_snippet.isValid()) return;
    m_snippet.name = text;
    scheduleAutoSave();
}

void SnippetEditor::onTabContentChanged() {
    if (m_snippet.isValid()) {
        int totalChars = 0;
        for (int i = 0; i < m_tabs->count(); ++i) {
            auto* ed = qobject_cast<QPlainTextEdit*>(m_tabs->widget(i));
            if (ed) totalChars += ed->toPlainText().length();
        }
        m_charCount->setText(QStringLiteral("%1 chars").arg(totalChars));
    }
    if (m_loading || !m_snippet.isValid()) return;
    scheduleAutoSave();
}

void SnippetEditor::onLanguageChanged(const QString& lang) {
    if (m_loading || !m_snippet.isValid()) return;
    int idx = m_tabs->currentIndex();
    if (idx >= 0 && idx < m_snippet.tabs.size()) {
        m_snippet.tabs[idx].language = lang;
        scheduleAutoSave();
    }
}

void SnippetEditor::onAddTab() {
    if (!m_snippet.isValid()) return;
    SnippetTab tab;
    tab.name     = QStringLiteral("Fragment %1").arg(m_snippet.tabs.size() + 1);
    tab.content  = "";
    tab.language = "plaintext";
    m_snippet.tabs.append(tab);

    auto* editor = new QPlainTextEdit(this);
    editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    editor->document()->setDefaultFont(QFont("JetBrains Mono,Cascadia Code,Fira Code,Consolas", 13));
    m_tabs->addTab(editor, tab.name);
    m_tabs->setCurrentIndex(m_tabs->count() - 1);
    connect(editor, &QPlainTextEdit::textChanged, this, &SnippetEditor::onTabContentChanged);
    scheduleAutoSave();
}

void SnippetEditor::onCloseTab(int index) {
    if (!m_snippet.isValid() || m_snippet.tabs.size() <= 1) return;
    m_tabs->removeTab(index);
    if (index < m_snippet.tabs.size())
        m_snippet.tabs.removeAt(index);
    scheduleAutoSave();
}

void SnippetEditor::onCopyContent() {
    auto* ed = qobject_cast<QPlainTextEdit*>(m_tabs->currentWidget());
    if (ed) QApplication::clipboard()->setText(ed->toPlainText());
}

void SnippetEditor::onAddTag() {
    QString tag = m_tagInput->text().trimmed().toLower();
    if (tag.isEmpty() || !m_snippet.isValid()) return;
    if (!m_snippet.tags.contains(tag)) {
        m_snippet.tags.append(tag);
        m_tagInput->clear();
        rebuildTagChips();
        scheduleAutoSave();
    }
}

void SnippetEditor::buildTagsBar() {
    m_tagsBar = new QWidget(this);
    m_tagsBar->setObjectName("TagsBar");
    m_tagsLayout = new QHBoxLayout(m_tagsBar);
    m_tagsLayout->setContentsMargins(8, 6, 8, 6);
    m_tagsLayout->setSpacing(6);

    m_tagInput = new QLineEdit(m_tagsBar);
    m_tagInput->setPlaceholderText("Add tag…");
    m_tagInput->setFixedWidth(120);
    m_tagInput->setStyleSheet(
        "background:#161b22; border:1px solid #30363d; border-radius:4px;"
        "padding:2px 8px; font-size:11px; color:#8b949e;");
    connect(m_tagInput, &QLineEdit::returnPressed, this, &SnippetEditor::onAddTag);

    m_tagsLayout->addWidget(m_tagInput);
    m_tagsLayout->addStretch();
}

void SnippetEditor::rebuildTagChips() {
    // Remove chip widgets (keep input and stretch)
    QList<QWidget*> chips;
    for (int i = 0; i < m_tagsLayout->count(); ++i) {
        auto* w = m_tagsLayout->itemAt(i)->widget();
        if (w && w != m_tagInput) chips << w;
    }
    for (auto* w : chips) { m_tagsLayout->removeWidget(w); delete w; }

    for (auto& tag : m_snippet.tags) {
        auto* chip = new QToolButton(m_tagsBar);
        chip->setText(QStringLiteral("# ") + tag + "  ×");
        chip->setStyleSheet(
            "QToolButton { background:#1f3352; color:#58a6ff; border:1px solid #2d4a6e;"
            "border-radius:4px; font-size:11px; padding:2px 6px; }"
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

void SnippetEditor::scheduleAutoSave() {
    m_dirty = true;
    m_saveTimer->start();
}

void SnippetEditor::autoSave() {
    if (!m_dirty || !m_snippet.isValid()) return;
    // Sync tab contents from widgets
    for (int i = 0; i < m_tabs->count() && i < m_snippet.tabs.size(); ++i) {
        auto* ed = qobject_cast<QPlainTextEdit*>(m_tabs->widget(i));
        if (ed) m_snippet.tabs[i].content = ed->toPlainText();
        m_snippet.tabs[i].name = m_tabs->tabText(i);
    }
    m_db->updateSnippet(m_snippet);
    m_dirty = false;
    emit snippetModified(m_snippet.id);
}
