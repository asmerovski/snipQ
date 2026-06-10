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
    void onContextMenu(const QPoint& pos);

private:
    enum SortMode { SortName, SortCreated, SortSize };

    void populateList(const QList<Snippet>& snippets);
    void filterList(const QString& query);
    QList<Snippet> sortedSnippets(QList<Snippet> list) const;

    Database*        m_db;
    QListWidget*     m_list;
    QLineEdit*       m_search;
    QLabel*          m_header;
    QComboBox*       m_sortCombo;
    QList<Snippet>   m_snippets;
    SidebarSelection m_sel;
    SortMode         m_sortMode = SortName;
    bool             m_isBin    = false;
};
