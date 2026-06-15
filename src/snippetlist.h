#pragma once
#include <QWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QToolButton>
#include "database.h"
#include "sidebar.h"

class SnippetList : public QWidget {
    Q_OBJECT

public:
    explicit SnippetList(Database* db, QWidget* parent = nullptr);

    void loadFor(const SidebarSelection& sel);
    void refresh();
    void selectSnippet(int snippetId);  // selects item in list without reloading
    void pinSnippet(int snippetId);     // survive refresh() until user clicks

signals:
    void snippetSelected(int snippetId);
    void newSnippetRequested();
    void deleteSnippetRequested(int snippetId);
    void restoreSnippetRequested(int snippetId);
    void toggleFavoriteRequested(int snippetId);

private slots:
    void onCurrentRowChanged(int row);
    void onSearchChanged(const QString& text);
    void onSortChanged(int index);
    void onToggleDirection();
    void onContextMenu(const QPoint& pos);

private:
    enum SortMode { SortName, SortCreated, SortModified, SortSize };

    void populateList(const QList<Snippet>& snippets);
    void filterList(const QString& query);
    QList<Snippet> sortedSnippets(QList<Snippet> list) const;
    void updateDirectionButton();

    Database*        m_db;
    QListWidget*     m_list;
    QLineEdit*       m_search;
    QLabel*          m_header;
    QComboBox*       m_sortCombo;
    QToolButton*     m_dirBtn;       // ↑ / ↓ toggle
    QList<Snippet>   m_snippets;
    SidebarSelection m_sel;
    SortMode         m_sortMode  = SortName;
    bool             m_ascending = true;
    bool             m_isBin     = false;
    int              m_pinnedId  = -1;  // persists across refresh() until user clicks
};
