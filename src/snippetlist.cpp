#include "snippetlist.h"
#include <QMenu>
#include <QLabel>
#include <QHBoxLayout>
#include <QToolButton>
#include <QScrollBar>

static const int RoleSnippetId = Qt::UserRole + 1;

// ── Custom list item widget ───────────────────────────────────────────────────

class SnippetListItem : public QWidget {
public:
    SnippetListItem(const Snippet& s, QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setCursor(Qt::PointingHandCursor);

        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(12, 8, 12, 8);
        lay->setSpacing(3);

        // ── Row 1: name + star ────────────────────────────────
        auto* top = new QHBoxLayout;
        top->setContentsMargins(0, 0, 0, 0);
        top->setSpacing(6);

        QString safeName = s.name.trimmed();
        if (safeName.isEmpty()) safeName = QStringLiteral("Untitled");

        auto* nameLabel = new QLabel(safeName, this);
        nameLabel->setStyleSheet(
            "font-weight:600; font-size:13px; color:#c9d1d9; background:transparent;");
        nameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        top->addWidget(nameLabel, 1);

        if (s.isFavorite) {
            auto* star = new QLabel(QStringLiteral("\u2605"), this);
            star->setStyleSheet("color:#e3b341; font-size:12px; background:transparent;");
            star->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            top->addWidget(star);
        }
        lay->addLayout(top);

        // ── Row 2: language chip or description ───────────────
        QString lang = s.language.trimmed();
        if (!lang.isEmpty() && lang != QStringLiteral("plaintext")) {
            auto* langLabel = new QLabel(lang, this);
            langLabel->setStyleSheet(
                "font-size:10px; color:#58a6ff; background-color:#1f3352;"
                "border-radius:4px; padding:1px 6px;");
            langLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            langLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            lay->addWidget(langLabel);
        } else if (!s.description.trimmed().isEmpty()) {
            QString desc = s.description.trimmed();
            if (desc.length() > 55) desc = desc.left(55) + QStringLiteral("\u2026");
            auto* descLabel = new QLabel(desc, this);
            descLabel->setStyleSheet(
                "color:#484f58; font-size:11px; background:transparent;");
            descLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            lay->addWidget(descLabel);
        }

        // ── Row 3: up to 3 tag chips ──────────────────────────
        if (!s.tags.isEmpty()) {
            auto* tagRow = new QHBoxLayout;
            tagRow->setContentsMargins(0, 0, 0, 0);
            tagRow->setSpacing(4);
            int shown = 0;
            for (const QString& t : s.tags) {
                if (shown++ >= 3) break;
                QString safeTag = t.trimmed();
                if (safeTag.isEmpty()) continue;
                auto* chip = new QLabel(QStringLiteral("# ") + safeTag, this);
                chip->setStyleSheet(
                    "font-size:10px; color:#388bfd; background:transparent; padding:0;");
                chip->setAttribute(Qt::WA_TransparentForMouseEvents, true);
                tagRow->addWidget(chip);
            }
            tagRow->addStretch();
            lay->addLayout(tagRow);
        }
    }
};

// ─── SnippetList ─────────────────────────────────────────────────────────────

SnippetList::SnippetList(Database* db, QWidget* parent)
    : QWidget(parent), m_db(db)
{
    setObjectName("SnippetList");
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    // ── Header row ───────────────────────────────────────────
    auto* headerRow = new QHBoxLayout;
    headerRow->setContentsMargins(12, 10, 8, 4);
    m_header = new QLabel("All Snippets", this);
    m_header->setStyleSheet("font-size:13px; font-weight:600; color:#8b949e;");
    headerRow->addWidget(m_header, 1);

    auto* addBtn = new QToolButton(this);
    addBtn->setText("+");
    addBtn->setToolTip("New Snippet");
    addBtn->setStyleSheet(
        "QToolButton{background:#1f6feb;color:#fff;border-radius:5px;"
        "font-size:16px;padding:0 6px;border:none;}"
        "QToolButton:hover{background:#388bfd;}");
    connect(addBtn, &QToolButton::clicked,
            this,   &SnippetList::newSnippetRequested);
    headerRow->addWidget(addBtn);
    lay->addLayout(headerRow);

    // ── Search ───────────────────────────────────────────────
    m_search = new QLineEdit(this);
    m_search->setObjectName("SnippetListSearch");
    m_search->setPlaceholderText("Search snippets\u2026");
    m_search->setClearButtonEnabled(true);
    lay->addWidget(m_search);

    // ── Sort row ─────────────────────────────────────────────
    auto* sortRow = new QHBoxLayout;
    sortRow->setContentsMargins(8, 4, 8, 4);
    sortRow->setSpacing(4);

    auto* sortLabel = new QLabel("Sort:", this);
    sortLabel->setStyleSheet("color:#484f58; font-size:11px;");
    sortRow->addWidget(sortLabel);

    m_sortCombo = new QComboBox(this);
    m_sortCombo->addItem("Name",     SortName);
    m_sortCombo->addItem("Created",  SortCreated);
    m_sortCombo->addItem("Modified", SortModified);
    m_sortCombo->addItem("Size",     SortSize);
    m_sortCombo->setStyleSheet(
        "QComboBox{background:#161b22;border:1px solid #30363d;border-radius:4px;"
        "padding:2px 6px;font-size:11px;color:#8b949e;min-width:80px;}"
        "QComboBox::drop-down{border:none;width:16px;}"
        "QComboBox QAbstractItemView{background:#161b22;border:1px solid #30363d;"
        "selection-background-color:#1f3352;}");
    sortRow->addWidget(m_sortCombo);

    // Asc / Desc toggle button
    m_dirBtn = new QToolButton(this);
    m_dirBtn->setToolTip("Toggle sort direction");
    m_dirBtn->setStyleSheet(
        "QToolButton{background:#161b22;border:1px solid #30363d;border-radius:4px;"
        "color:#8b949e;font-size:12px;padding:2px 6px;}"
        "QToolButton:hover{background:#21262d;color:#c9d1d9;}");
    updateDirectionButton();
    connect(m_dirBtn, &QToolButton::clicked,
            this,     &SnippetList::onToggleDirection);
    sortRow->addWidget(m_dirBtn);

    sortRow->addStretch();
    lay->addLayout(sortRow);

    // ── List ─────────────────────────────────────────────────
    m_list = new QListWidget(this);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setCursor(Qt::PointingHandCursor);
    m_list->viewport()->setCursor(Qt::PointingHandCursor);
    lay->addWidget(m_list);

    connect(m_list,      &QListWidget::currentRowChanged,
            this,        &SnippetList::onCurrentRowChanged);
    connect(m_search,    &QLineEdit::textChanged,
            this,        &SnippetList::onSearchChanged);
    connect(m_sortCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this,        &SnippetList::onSortChanged);
    connect(m_list,      &QListWidget::customContextMenuRequested,
            this,        &SnippetList::onContextMenu);
}

void SnippetList::pinSnippet(int snippetId)
{
    m_pinnedId = snippetId;
}

void SnippetList::selectSnippet(int snippetId)
{
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem* item = m_list->item(i);
        if (item && item->data(RoleSnippetId).toInt() == snippetId) {
            // Block signal so onCurrentRowChanged doesn't fire and re-open
            // the snippet redundantly (mainwindow already calls loadSnippet)
            QSignalBlocker sb(m_list);
            m_list->setCurrentItem(item);
            m_list->scrollToItem(item, QAbstractItemView::PositionAtCenter);
            return;
        }
    }
}

void SnippetList::loadFor(const SidebarSelection& sel)
{
    m_sel   = sel;
    m_isBin = (sel.type == SidebarSelection::Bin);
    refresh();
}

void SnippetList::refresh()
{
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
    };
    m_header->setText(
        m_sel.type == SidebarSelection::Tag
            ? QStringLiteral("# ") + m_sel.tag
            : headers.value((int)m_sel.type, "Snippets"));

    filterList(m_search->text());
}

QList<Snippet> SnippetList::sortedSnippets(QList<Snippet> list) const
{
    auto cmp = [&](const Snippet& a, const Snippet& b) -> bool {
        switch (m_sortMode) {
            case SortName:
                return a.name.toLower() < b.name.toLower();
            case SortCreated:
                return a.createdAt < b.createdAt;
            case SortModified:
                return a.updatedAt < b.updatedAt;
            case SortSize:
                return a.content.length() < b.content.length();
        }
        return false;
    };

    std::sort(list.begin(), list.end(), [&](const Snippet& a, const Snippet& b) {
        return m_ascending ? cmp(a, b) : cmp(b, a);
    });
    return list;
}

void SnippetList::populateList(const QList<Snippet>& snippets)
{
    // Prefer the pinned ID over whatever currentItem happens to be.
    // The pin survives multiple refresh() calls (e.g. queued onDataChanged)
    // until it's found and rendered — then it's cleared.
    int selId = (m_pinnedId >= 0) ? m_pinnedId
              : (m_list->currentItem()
                 ? m_list->currentItem()->data(RoleSnippetId).toInt()
                 : -1);

    int scrollY = m_list->verticalScrollBar()->value();

    m_list->blockSignals(true);
    m_list->clear();

    QListWidgetItem* toSelect = nullptr;
    for (const Snippet& s : sortedSnippets(snippets)) {
        auto* item = new QListWidgetItem();
        item->setData(RoleSnippetId, s.id);
        item->setSizeHint(QSize(0, 68));
        m_list->addItem(item);
        m_list->setItemWidget(item, new SnippetListItem(s, nullptr));
        if (s.id == selId) toSelect = item;
    }

    m_list->blockSignals(false);

    if (toSelect) {
        QSignalBlocker sb(m_list);
        m_list->setCurrentItem(toSelect);
        m_list->scrollToItem(toSelect, QAbstractItemView::PositionAtCenter);
        m_pinnedId = -1;  // consumed — item is visible and selected
    } else {
        // selId not found in this list (e.g. wrong sidebar section) —
        // keep the pin alive so a later refresh in the right section finds it
        m_list->verticalScrollBar()->setValue(scrollY);
    }
}

void SnippetList::filterList(const QString& query)
{
    if (query.trimmed().isEmpty()) {
        populateList(m_snippets);
        return;
    }
    QString q = query.toLower();
    QList<Snippet> filtered;
    for (const Snippet& s : m_snippets) {
        if (s.name.toLower().contains(q)        ||
            s.description.toLower().contains(q) ||
            s.content.toLower().contains(q)     ||
            s.tags.join(' ').toLower().contains(q))
            filtered << s;
    }
    populateList(filtered);
}

void SnippetList::updateDirectionButton()
{
    m_dirBtn->setText(m_ascending
        ? QStringLiteral("\u2191 Asc")
        : QStringLiteral("\u2193 Desc"));
}

void SnippetList::onCurrentRowChanged(int)
{
    m_pinnedId = -1;  // user clicked — pin no longer needed
    auto* item = m_list->currentItem();
    if (item) emit snippetSelected(item->data(RoleSnippetId).toInt());
}

void SnippetList::onSearchChanged(const QString& text)
{
    filterList(text);
}

void SnippetList::onSortChanged(int)
{
    m_sortMode = static_cast<SortMode>(m_sortCombo->currentData().toInt());
    filterList(m_search->text());
}

void SnippetList::onToggleDirection()
{
    m_ascending = !m_ascending;
    updateDirectionButton();
    filterList(m_search->text());
}

void SnippetList::onContextMenu(const QPoint& pos)
{
    auto* item = m_list->itemAt(pos);
    if (!item) return;
    int sid = item->data(RoleSnippetId).toInt();

    QMenu menu(this);
    auto* actFav    = menu.addAction("Toggle Favourite");
    menu.addSeparator();
    QAction* actRestore = nullptr;
    QAction* actDelete  = nullptr;

    if (m_isBin) {
        actRestore = menu.addAction("Restore");
        actDelete  = menu.addAction("Delete Permanently");
    } else {
        actDelete  = menu.addAction("Move to Bin");
    }

    auto* chosen = menu.exec(m_list->viewport()->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == actFav)                   emit toggleFavoriteRequested(sid);
    if (actRestore && chosen == actRestore) emit restoreSnippetRequested(sid);
    if (actDelete  && chosen == actDelete)  emit deleteSnippetRequested(sid);
}
