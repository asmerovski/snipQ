#include "snippetlist.h"
#include <QMenu>
#include <QLabel>
#include <QHBoxLayout>
#include <QToolButton>

static const int RoleSnippetId = Qt::UserRole + 1;

// ── Custom list item widget ───────────────────────────────────────────────────
class SnippetListItem : public QWidget {
public:
    SnippetListItem(const Snippet& s, QWidget* parent = nullptr) : QWidget(parent) {
        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(12, 10, 12, 10);
        lay->setSpacing(4);

        auto* top = new QHBoxLayout;
        top->setContentsMargins(0,0,0,0);

        auto* nameLabel = new QLabel(s.name, this);
        nameLabel->setStyleSheet("font-weight: 600; font-size: 13px; color: #c9d1d9;");
        top->addWidget(nameLabel, 1);

        if (s.isFavorite) {
            auto* star = new QLabel("★", this);
            star->setStyleSheet("color: #e3b341; font-size: 12px;");
            top->addWidget(star);
        }
        lay->addLayout(top);

        // Language chips from first tab
        if (!s.tabs.isEmpty() && s.tabs[0].language != "plaintext") {
            auto* lang = new QLabel(s.tabs[0].language, this);
            lang->setStyleSheet(
                "font-size: 10px; color: #58a6ff;"
                "background-color: #1f3352; border-radius: 4px;"
                "padding: 1px 6px; max-width: 80px;");
            lay->addWidget(lang);
        } else if (!s.description.isEmpty()) {
            QString desc = s.description;
            if (desc.length() > 60) desc = desc.left(60) + "…";
            auto* descLabel = new QLabel(desc, this);
            descLabel->setStyleSheet("color: #484f58; font-size: 11px;");
            lay->addWidget(descLabel);
        }

        // Tags
        if (!s.tags.isEmpty()) {
            auto* tagRow = new QHBoxLayout;
            tagRow->setContentsMargins(0,0,0,0);
            tagRow->setSpacing(4);
            int shown = 0;
            for (auto& t : s.tags) {
                if (shown++ >= 3) break;
                auto* chip = new QLabel(QStringLiteral("#") + t, this);
                chip->setStyleSheet(
                    "font-size: 10px; color: #388bfd;"
                    "background: transparent; padding: 0;");
                tagRow->addWidget(chip);
            }
            tagRow->addStretch();
            lay->addLayout(tagRow);
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────

SnippetList::SnippetList(Database* db, QWidget* parent)
    : QWidget(parent), m_db(db) {
    setObjectName("SnippetList");
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    // Header row
    auto* headerRow = new QHBoxLayout;
    headerRow->setContentsMargins(12, 10, 8, 4);
    m_header = new QLabel("All Snippets", this);
    m_header->setStyleSheet("font-size:13px; font-weight:600; color:#8b949e;");
    headerRow->addWidget(m_header, 1);

    auto* addBtn = new QToolButton(this);
    addBtn->setText("+");
    addBtn->setToolTip("New Snippet");
    addBtn->setStyleSheet(
        "QToolButton { background:#1f6feb; color:#fff; border-radius:5px;"
        "font-size:16px; padding:0 6px; border:none; }"
        "QToolButton:hover { background:#388bfd; }");
    connect(addBtn, &QToolButton::clicked, this, &SnippetList::newSnippetRequested);
    headerRow->addWidget(addBtn);
    lay->addLayout(headerRow);

    m_search = new QLineEdit(this);
    m_search->setObjectName("SnippetListSearch");
    m_search->setPlaceholderText("Search snippets…");
    m_search->setClearButtonEnabled(true);
    lay->addWidget(m_search);

    m_list = new QListWidget(this);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    lay->addWidget(m_list);

    connect(m_list, &QListWidget::currentRowChanged,
            this,   &SnippetList::onCurrentRowChanged);
    connect(m_search, &QLineEdit::textChanged,
            this,     &SnippetList::onSearchChanged);
    connect(m_list, &QListWidget::customContextMenuRequested,
            this,   &SnippetList::onContextMenu);
}

void SnippetList::loadFor(const SidebarSelection& sel) {
    m_sel = sel;
    m_isBin = (sel.type == SidebarSelection::Bin);
    refresh();
}

void SnippetList::refresh() {
    QList<Snippet> snips;
    switch (m_sel.type) {
        case SidebarSelection::AllSnippets: snips = m_db->allSnippets();                    break;
        case SidebarSelection::Favourites:  snips = m_db->favoriteSnippets();               break;
        case SidebarSelection::Bin:         snips = m_db->deletedSnippets();                break;
        case SidebarSelection::Folder:      snips = m_db->snippetsByFolder(m_sel.folderId); break;
        case SidebarSelection::Tag:         snips = m_db->snippetsByTag(m_sel.tag);         break;
    }
    m_snippets = snips;

    static const QMap<int,QString> headers = {
        {SidebarSelection::AllSnippets, "All Snippets"},
        {SidebarSelection::Favourites,  "Favourites"},
        {SidebarSelection::Bin,         "Bin"},
        {SidebarSelection::Folder,      "Folder"},
        {SidebarSelection::Tag,         QString()},
    };
    if (m_sel.type == SidebarSelection::Tag)
        m_header->setText(QStringLiteral("#") + m_sel.tag);
    else
        m_header->setText(headers.value((int)m_sel.type, "Snippets"));

    filterList(m_search->text());
}

void SnippetList::populateList(const QList<Snippet>& snippets) {
    // Remember selection
    int selId = -1;
    if (auto* cur = m_list->currentItem())
        selId = cur->data(RoleSnippetId).toInt();

    m_list->clear();
    for (auto& s : snippets) {
        auto* item = new QListWidgetItem(m_list);
        item->setData(RoleSnippetId, s.id);
        item->setSizeHint(QSize(0, 76));
        auto* w = new SnippetListItem(s, m_list);
        m_list->setItemWidget(item, w);
        if (s.id == selId) m_list->setCurrentItem(item);
    }
}

void SnippetList::filterList(const QString& query) {
    if (query.trimmed().isEmpty()) {
        populateList(m_snippets);
        return;
    }
    QString q = query.toLower();
    QList<Snippet> filtered;
    for (auto& s : m_snippets) {
        if (s.name.toLower().contains(q) ||
            s.description.toLower().contains(q) ||
            s.tags.join(" ").toLower().contains(q)) {
            filtered << s;
        }
    }
    populateList(filtered);
}

void SnippetList::onCurrentRowChanged(int /*row*/) {
    auto* item = m_list->currentItem();
    if (item) emit snippetSelected(item->data(RoleSnippetId).toInt());
}

void SnippetList::onSearchChanged(const QString& text) {
    filterList(text);
}

void SnippetList::onContextMenu(const QPoint& pos) {
    auto* item = m_list->itemAt(pos);
    if (!item) return;
    int sid = item->data(RoleSnippetId).toInt();

    QMenu menu(this);
    QAction* actFav     = menu.addAction("Toggle Favourite");
    menu.addSeparator();
    QAction* actRestore = nullptr;
    QAction* actDelete  = nullptr;

    if (m_isBin) {
        actRestore = menu.addAction("Restore");
        actDelete  = menu.addAction("Delete Permanently");
        actDelete->setProperty("danger", true);
    } else {
        actDelete = menu.addAction("Move to Bin");
    }

    auto* chosen = menu.exec(m_list->viewport()->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == actFav)     emit toggleFavoriteRequested(sid);
    if (actRestore && chosen == actRestore) emit restoreSnippetRequested(sid);
    if (chosen == actDelete) {
        if (m_isBin) emit deleteSnippetRequested(sid); // permanent
        else         emit deleteSnippetRequested(sid);
    }
}
