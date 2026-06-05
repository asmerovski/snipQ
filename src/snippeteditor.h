#pragma once
#include <QWidget>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QToolButton>

#include "database.h"

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
    void onTabContentChanged();
    void onLanguageChanged(const QString& lang);
    void onAddTab();
    void onCloseTab(int index);
    void onAddTag();
    void onCopyContent();
    void autoSave();

private:
    void buildTagsBar();
    void rebuildTagChips();
    void scheduleAutoSave();

    Database*       m_db;
    Snippet         m_snippet;
    bool            m_loading = false;

    QLineEdit*      m_titleEdit;
    QTabWidget*     m_tabs;
    QComboBox*      m_langCombo;
    QWidget*        m_tagsBar;
    QHBoxLayout*    m_tagsLayout;
    QLineEdit*      m_tagInput;
    QLabel*         m_charCount;
    QTimer*         m_saveTimer;
    bool            m_dirty = false;
};
