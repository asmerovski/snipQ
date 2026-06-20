#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QToolButton>
#include <QTimer>
#include "database.h"
#include "codeeditor.h"
#include "highlighter.h"

class SnippetEditor : public QWidget {
    Q_OBJECT

public:
    explicit SnippetEditor(Database* db, QWidget* parent = nullptr);

    void loadSnippet(int id);
    void clearEditor();

signals:
    void snippetModified(int snippetId);

private slots:
    void onTitleChanged(const QString& text);
    void onDescriptionChanged(const QString& text);
    void onContentChanged();
    void onLanguageChanged(const QString& lang);
    void onAddTag();
    void onCopyContent();
    void onFormatCode();
    void autoSave();

private:
    void buildTagsBar();
    void rebuildTagChips();
    void scheduleAutoSave();
    void applyLanguage(const QString& lang);
    void updateTitleCounter(const QString& text);
    void updateDescCounter(const QString& text);

    Database*       m_db;
    Snippet         m_snippet;
    bool            m_loading = false;
    bool            m_dirty   = false;

    QLineEdit*      m_titleEdit;
    QLineEdit*      m_descEdit;
    QLabel*         m_titleCounter;
    QLabel*         m_descCounter;
    QLabel*         m_timestampBar;  // ← created / modified line
    CodeEditor*     m_editor;
    QComboBox*      m_langCombo;
    QWidget*        m_tagsBar;
    QHBoxLayout*    m_tagsLayout;
    QLineEdit*      m_tagInput;
    QLabel*         m_charCount;
    QTimer*         m_saveTimer;
    Highlighter*    m_highlighter = nullptr;
};
