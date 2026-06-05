#include "sidebar.h"
#include <QMenu>
#include <QInputDialog>
#include <QHeaderView>
#include <QApplication>
#include <QStyle>

// Custom roles stored in tree items
static const int RoleType     = Qt::UserRole + 1; // "lib_all","lib_fav","bin","folder","tag","section"
static const int RoleFolderId = Qt::UserRole + 2;
static const int RoleTagName  = Qt::UserRole + 3;

Sidebar::Sidebar(Database* db, QWidget* parent) : QWidget(parent), m_db(db) {
    setObjectName("Sidebar");

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    // App logo / title area
    auto* header = new QLabel("  \u2b21  snipQ", this);
    header->setStyleSheet("font-size:14px; font-weight:700; color:#58a6ff;"
                          "padding:14px 12px 10px 12px; letter-spacing:1px;");
    lay->addWidget(header);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setAnimated(true);
    m_tree->setExpandsOnDoubleClick(false);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setIndentation(16);
    lay->addWidget(m_tree);

    connect(m_tree, &QTreeWidget::itemClicked,
            this,   &Sidebar::onItemClicked);
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this,   &Sidebar::onContextMenu);

    buildTree();

    if (m_allItem)
        m_tree->setCurrentItem(m_allItem);
}

void Sidebar::refresh() {
    auto* prev = m_tree->currentItem();
    QVariant prevType;
    int      prevFold = -1;
    QString  prevTag;
    if (prev) {
        prevType  = prev->data(0, RoleType);
        prevFold  = prev->data(0, RoleFolderId).toInt();
        prevTag   = prev->data(0, RoleTagName).toString();
    }

    buildTree();

    if (prevType.isValid()) {
        QString t = prevType.toString();
        if (t == "folder" && prevFold >= 0) {
            QTreeWidgetItemIterator it(m_tree);
            while (*it) {
                if ((*it)->data(0, RoleType).toString() == "folder" &&
                    (*it)->data(0, RoleFolderId).toInt() == prevFold) {
                    m_tree->setCurrentItem(*it);
                    return;
                }
                ++it;
            }
        } else if (t == "tag") {
            QTreeWidgetItemIterator it(m_tree);
            while (*it) {
                if ((*it)->data(0, RoleType).toString() == "tag" &&
                    (*it)->data(0, RoleTagName).toString() == prevTag) {
                    m_tree->setCurrentItem(*it);
                    return;
                }
                ++it;
            }
        }
    }
    if (m_allItem) m_tree->setCurrentItem(m_allItem);
}

void Sidebar::buildTree() {
    m_tree->clear();
    m_libSection = m_foldSection = m_tagSection = nullptr;
    m_allItem = m_favItem = m_binItem = nullptr;

    // Shared section header font
    QFont secFont;
    secFont.setPointSizeF(secFont.pointSizeF() * 0.8);
    secFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);

    // ── LIBRARY ──────────────────────────────────────────────
    m_libSection = new QTreeWidgetItem();
    m_libSection->setText(0, "LIBRARY");
    m_libSection->setData(0, RoleType, "section");
    m_libSection->setFlags(Qt::ItemIsEnabled);
    m_libSection->setForeground(0, QColor("#484f58"));
    m_libSection->setFont(0, secFont);
    m_tree->addTopLevelItem(m_libSection);

    m_favItem = new QTreeWidgetItem(m_libSection);
    m_favItem->setText(0, "  \u2605  Favourites");
    m_favItem->setData(0, RoleType, "lib_fav");
    m_favItem->setForeground(0, QColor("#e3b341"));

    m_allItem = new QTreeWidgetItem(m_libSection);
    m_allItem->setText(0, "  \u25c8  All Snippets");
    m_allItem->setData(0, RoleType, "lib_all");

    m_binItem = new QTreeWidgetItem(m_libSection);
    m_binItem->setText(0, "  \u2297  Bin");
    m_binItem->setData(0, RoleType, "bin");
    m_binItem->setForeground(0, QColor("#6e7681"));

    m_libSection->setExpanded(true);

    // ── FOLDERS ───────────────────────────────────────────────
    m_foldSection = new QTreeWidgetItem();
    m_foldSection->setText(0, "FOLDERS");
    m_foldSection->setData(0, RoleType, "section");
    m_foldSection->setFlags(Qt::ItemIsEnabled);
    m_foldSection->setForeground(0, QColor("#484f58"));
    m_foldSection->setFont(0, secFont);
    m_tree->addTopLevelItem(m_foldSection);

    auto folders = m_db->allFolders();
    QMap<int, QTreeWidgetItem*> foldMap;
    QList<Folder> roots, children;
    for (auto& f : folders) {
        if (f.parentId <= 0) roots << f;
        else children << f;
    }
    for (auto& f : roots) {
        auto* item = new QTreeWidgetItem(m_foldSection);
        item->setText(0, "  \u25b8  " + f.name);
        item->setData(0, RoleType, "folder");
        item->setData(0, RoleFolderId, f.id);
        foldMap[f.id] = item;
    }
    for (auto& f : children) {
        auto* parent = foldMap.value(f.parentId, m_foldSection);
        auto* item = new QTreeWidgetItem(parent);
        item->setText(0, "  \u25b8  " + f.name);
        item->setData(0, RoleType, "folder");
        item->setData(0, RoleFolderId, f.id);
        foldMap[f.id] = item;
    }
    m_foldSection->setExpanded(true);

    // ── TAGS ──────────────────────────────────────────────────
    m_tagSection = new QTreeWidgetItem();
    m_tagSection->setText(0, "TAGS");
    m_tagSection->setData(0, RoleType, "section");
    m_tagSection->setFlags(Qt::ItemIsEnabled);
    m_tagSection->setForeground(0, QColor("#484f58"));
    m_tagSection->setFont(0, secFont);
    m_tree->addTopLevelItem(m_tagSection);

    auto tags = m_db->allTags();
    for (auto& tag : tags) {
        auto* item = new QTreeWidgetItem(m_tagSection);
        item->setText(0, "  #  " + tag);
        item->setData(0, RoleType, "tag");
        item->setData(0, RoleTagName, tag);
        item->setForeground(0, QColor("#58a6ff"));
    }
    m_tagSection->setExpanded(true);
}

void Sidebar::onItemClicked(QTreeWidgetItem* item, int /*col*/) {
    if (!item) return;
    QString type = item->data(0, RoleType).toString();
    if (type == "section") return;

    SidebarSelection sel;
    if      (type == "lib_all") sel.type = SidebarSelection::AllSnippets;
    else if (type == "lib_fav") sel.type = SidebarSelection::Favourites;
    else if (type == "bin")     sel.type = SidebarSelection::Bin;
    else if (type == "folder") {
        sel.type     = SidebarSelection::Folder;
        sel.folderId = item->data(0, RoleFolderId).toInt();
    } else if (type == "tag") {
        sel.type = SidebarSelection::Tag;
        sel.tag  = item->data(0, RoleTagName).toString();
    }
    emit selectionChanged(sel);
}

void Sidebar::onContextMenu(const QPoint& pos) {
    auto* item = m_tree->itemAt(pos);
    QMenu menu(this);

    auto* actNewFolder = menu.addAction("New Folder");
    QAction* actRename = nullptr;
    QAction* actDelete = nullptr;

    if (item && item->data(0, RoleType).toString() == "folder") {
        int fid = item->data(0, RoleFolderId).toInt();
        menu.addSeparator();
        actRename = menu.addAction("Rename Folder");
        actDelete = menu.addAction("Delete Folder");
        actDelete->setProperty("folderId", fid);
        actRename->setProperty("folderId", fid);
    }

    int parentFid = -1;
    if (item && item->data(0, RoleType).toString() == "folder")
        parentFid = item->data(0, RoleFolderId).toInt();

    auto* chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == actNewFolder) {
        emit newFolderRequested(parentFid);
    } else if (actRename && chosen == actRename) {
        emit renameFolderRequested(chosen->property("folderId").toInt());
    } else if (actDelete && chosen == actDelete) {
        emit deleteFolderRequested(chosen->property("folderId").toInt());
    }
}
