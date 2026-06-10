#pragma once
#include <QWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QSettings>
#include "database.h"

struct SidebarSelection {
    enum Type { AllSnippets, Favourites, Bin, Folder, Tag };
    Type    type     = AllSnippets;
    int     folderId = -1;
    QString tag;
};

class Sidebar : public QWidget {
    Q_OBJECT

public:
    explicit Sidebar(Database* db, QWidget* parent = nullptr);
    void refresh();

signals:
    void selectionChanged(SidebarSelection sel);
    void newFolderRequested(int parentFolderId);
    void renameFolderRequested(int folderId);
    void deleteFolderRequested(int folderId);

private slots:
    void onItemClicked(QTreeWidgetItem* item, int col);
    void onItemExpanded(QTreeWidgetItem* item);
    void onItemCollapsed(QTreeWidgetItem* item);
    void onContextMenu(const QPoint& pos);

private:
    void buildTree();
    void saveExpandState();
    void restoreExpandState();
    void updateSectionLabel(QTreeWidgetItem* section);

    Database*        m_db;
    QTreeWidget*     m_tree;

    QTreeWidgetItem* m_libSection  = nullptr;
    QTreeWidgetItem* m_foldSection = nullptr;
    QTreeWidgetItem* m_tagSection  = nullptr;
    QTreeWidgetItem* m_allItem     = nullptr;
    QTreeWidgetItem* m_favItem     = nullptr;
    QTreeWidgetItem* m_binItem     = nullptr;

    QSettings        m_settings;
};
