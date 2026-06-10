#include "sidebar.h"
#include <QMenu>
#include <QHeaderView>

static const int RoleType     = Qt::UserRole + 1;
static const int RoleFolderId = Qt::UserRole + 2;
static const int RoleTagName  = Qt::UserRole + 3;
static const int RoleSection  = Qt::UserRole + 4; // section key for settings

// Section header item — selectable so it receives click events for toggling
static QTreeWidgetItem* makeSectionItem(const QString& key, bool expanded) {
    auto* item = new QTreeWidgetItem();
    item->setData(0, RoleType,    QString("section"));
    item->setData(0, RoleSection, key);
    // Clickable but not selectable as a snippet target
    item->setFlags(Qt::ItemIsEnabled);
    item->setForeground(0, QColor("#484f58"));

    QFont f;
    f.setPointSizeF(f.pointSizeF() * 0.8);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
    item->setFont(0, f);

    // Chevron + label — updated by updateSectionLabel()
    Q_UNUSED(expanded)
    return item;
}

// ─────────────────────────────────────────────────────────────────────────────

Sidebar::Sidebar(Database* db, QWidget* parent)
    : QWidget(parent), m_db(db),
      m_settings("snipQ", "snipQ")
{
    setObjectName("Sidebar");

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    auto* header = new QLabel("  \u2b21  snipQ", this);
    header->setStyleSheet("font-size:14px; font-weight:700; color:#58a6ff;"
                          "padding:14px 12px 10px 12px; letter-spacing:1px;");
    lay->addWidget(header);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(false);   // we draw our own chevrons
    m_tree->setAnimated(true);
    m_tree->setExpandsOnDoubleClick(false);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setIndentation(18);
    lay->addWidget(m_tree);

    connect(m_tree, &QTreeWidget::itemClicked,
            this,   &Sidebar::onItemClicked);
    connect(m_tree, &QTreeWidget::itemExpanded,
            this,   &Sidebar::onItemExpanded);
    connect(m_tree, &QTreeWidget::itemCollapsed,
            this,   &Sidebar::onItemCollapsed);
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this,   &Sidebar::onContextMenu);

    buildTree();

    if (m_allItem)
        m_tree->setCurrentItem(m_allItem);
}

// ─── Public ──────────────────────────────────────────────────────────────────

void Sidebar::refresh()
{
    // Remember current selection
    auto* prev = m_tree->currentItem();
    QVariant prevType;
    int      prevFold = -1;
    QString  prevTag;
    if (prev) {
        prevType = prev->data(0, RoleType);
        prevFold = prev->data(0, RoleFolderId).toInt();
        prevTag  = prev->data(0, RoleTagName).toString();
    }

    buildTree();   // expand state is restored inside buildTree

    // Restore selection
    if (prevType.isValid()) {
        QString t = prevType.toString();
        QTreeWidgetItemIterator it(m_tree);
        while (*it) {
            QString itype = (*it)->data(0, RoleType).toString();
            if (t == "folder" && itype == "folder" &&
                (*it)->data(0, RoleFolderId).toInt() == prevFold) {
                m_tree->setCurrentItem(*it); return;
            }
            if (t == "tag" && itype == "tag" &&
                (*it)->data(0, RoleTagName).toString() == prevTag) {
                m_tree->setCurrentItem(*it); return;
            }
            ++it;
        }
    }
    if (m_allItem) m_tree->setCurrentItem(m_allItem);
}

// ─── Private ─────────────────────────────────────────────────────────────────

void Sidebar::buildTree()
{
    m_tree->clear();
    m_libSection = m_foldSection = m_tagSection = nullptr;
    m_allItem = m_favItem = m_binItem = nullptr;

    // ── LIBRARY ──────────────────────────────────────────────
    m_libSection = makeSectionItem("lib", true);
    m_tree->addTopLevelItem(m_libSection);

    m_favItem = new QTreeWidgetItem(m_libSection);
    m_favItem->setText(0, "  \u2605  Favourites");
    m_favItem->setData(0, RoleType, "lib_fav");
    m_favItem->setForeground(0, QColor("#e3b341"));
    m_favItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

    m_allItem = new QTreeWidgetItem(m_libSection);
    m_allItem->setText(0, "  \u25c8  All Snippets");
    m_allItem->setData(0, RoleType, "lib_all");
    m_allItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

    m_binItem = new QTreeWidgetItem(m_libSection);
    m_binItem->setText(0, "  \u2297  Bin");
    m_binItem->setData(0, RoleType, "bin");
    m_binItem->setForeground(0, QColor("#6e7681"));
    m_binItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

    // ── FOLDERS ───────────────────────────────────────────────
    m_foldSection = makeSectionItem("folders", true);
    m_tree->addTopLevelItem(m_foldSection);

    auto folders = m_db->allFolders();
    QMap<int, QTreeWidgetItem*> foldMap;
    for (auto& f : folders) {
        QTreeWidgetItem* parentItem = (f.parentId > 0)
            ? foldMap.value(f.parentId, m_foldSection)
            : m_foldSection;
        auto* item = new QTreeWidgetItem(parentItem);
        item->setText(0, "  \u25b8  " + f.name);
        item->setData(0, RoleType, "folder");
        item->setData(0, RoleFolderId, f.id);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        foldMap[f.id] = item;
    }

    // ── TAGS ──────────────────────────────────────────────────
    m_tagSection = makeSectionItem("tags", true);
    m_tree->addTopLevelItem(m_tagSection);

    for (const QString& tag : m_db->allTags()) {
        auto* item = new QTreeWidgetItem(m_tagSection);
        item->setText(0, "  #  " + tag);
        item->setData(0, RoleType, "tag");
        item->setData(0, RoleTagName, tag);
        item->setForeground(0, QColor("#58a6ff"));
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    }

    // Restore expand/collapse state from settings, then update labels
    restoreExpandState();
}

void Sidebar::updateSectionLabel(QTreeWidgetItem* section)
{
    if (!section) return;
    bool expanded = section->isExpanded();
    QString key = section->data(0, RoleSection).toString();

    QString chevron = expanded ? "\u25bc" : "\u25b6";   // ▼ / ▶
    QString label;
    if      (key == "lib")     label = "LIBRARY";
    else if (key == "folders") label = "FOLDERS";
    else if (key == "tags")    label = "TAGS";

    section->setText(0, QStringLiteral("  %1  %2").arg(chevron, label));
}

void Sidebar::saveExpandState()
{
    auto save = [&](QTreeWidgetItem* item, const QString& key) {
        if (item) m_settings.setValue("sidebar/expanded/" + key, item->isExpanded());
    };
    save(m_libSection,  "lib");
    save(m_foldSection, "folders");
    save(m_tagSection,  "tags");
}

void Sidebar::restoreExpandState()
{
    auto restore = [&](QTreeWidgetItem* item, const QString& key) {
        if (!item) return;
        bool expanded = m_settings.value("sidebar/expanded/" + key, true).toBool();
        item->setExpanded(expanded);
        updateSectionLabel(item);
    };
    restore(m_libSection,  "lib");
    restore(m_foldSection, "folders");
    restore(m_tagSection,  "tags");
}

// ─── Slots ───────────────────────────────────────────────────────────────────

void Sidebar::onItemClicked(QTreeWidgetItem* item, int /*col*/)
{
    if (!item) return;
    QString type = item->data(0, RoleType).toString();

    // Section header clicked → toggle expand/collapse
    if (type == "section") {
        item->setExpanded(!item->isExpanded());
        updateSectionLabel(item);
        saveExpandState();
        return;
    }

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
    } else {
        return;
    }
    emit selectionChanged(sel);
}

void Sidebar::onItemExpanded(QTreeWidgetItem* item)
{
    if (item->data(0, RoleType).toString() == "section") {
        updateSectionLabel(item);
        saveExpandState();
    }
}

void Sidebar::onItemCollapsed(QTreeWidgetItem* item)
{
    if (item->data(0, RoleType).toString() == "section") {
        updateSectionLabel(item);
        saveExpandState();
    }
}

void Sidebar::onContextMenu(const QPoint& pos)
{
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
        actRename->setProperty("folderId", fid);
        actDelete->setProperty("folderId", fid);
    }

    int parentFid = (item && item->data(0, RoleType).toString() == "folder")
        ? item->data(0, RoleFolderId).toInt() : -1;

    auto* chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == actNewFolder)                       emit newFolderRequested(parentFid);
    else if (actRename && chosen == actRename)        emit renameFolderRequested(actRename->property("folderId").toInt());
    else if (actDelete && chosen == actDelete)        emit deleteFolderRequested(actDelete->property("folderId").toInt());
}
