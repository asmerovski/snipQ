#include "sidebar.h"
#include <QPainter>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QEnterEvent>
#include <QMenu>
#include <QScrollArea>
#include <QLabel>

// ═══════════════════════════════════════════════════════════════════════════
// SidebarItem
// ═══════════════════════════════════════════════════════════════════════════

SidebarItem::SidebarItem(const QString& label,
                         const QString& itemType,
                         QWidget* parent)
    : QWidget(parent), m_type(itemType)
{
    setCursor(Qt::PointingHandCursor);
    setFixedHeight(26);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(20, 0, 8, 0);
    lay->setSpacing(0);

    m_label = new QLabel(label, this);
    // Explicit color — does not depend on global QSS being applied
    m_label->setStyleSheet("background:transparent; color:#c9d1d9;");
    m_label->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    lay->addWidget(m_label);
}

void SidebarItem::setLabelColor(const QString& color)
{
    m_label->setStyleSheet(
        QStringLiteral("background:transparent; color:%1;").arg(color));
}

void SidebarItem::setSelected(bool on)
{
    m_selected = on;
    update();
}

void SidebarItem::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) { /* handled on release */ }
    QWidget::mousePressEvent(e);
}

void SidebarItem::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && rect().contains(e->pos()))
        emit clicked(this);
    QWidget::mouseReleaseEvent(e);
}

void SidebarItem::contextMenuEvent(QContextMenuEvent* e)
{
    emit contextMenuRequested(this, e->globalPos());
}

void SidebarItem::enterEvent(QEnterEvent* e)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(e);
}

void SidebarItem::leaveEvent(QEvent* e)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(e);
}

void SidebarItem::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if (m_selected) {
        p.fillRect(rect(), QColor("#1f3352"));
        p.fillRect(0, 0, 3, height(), QColor("#388bfd")); // left accent bar
    } else if (m_hovered) {
        p.fillRect(rect(), QColor("#21262d"));
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// SidebarSection
// ═══════════════════════════════════════════════════════════════════════════

SidebarSection::SidebarSection(const QString& title,
                               const QString& key,
                               QSettings*     settings,
                               QWidget*       parent)
    : QWidget(parent), m_key(key), m_settings(settings)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* outerLay = new QVBoxLayout(this);
    outerLay->setContentsMargins(0, 0, 0, 0);
    outerLay->setSpacing(0);

    m_header = new QToolButton(this);
    m_header->setText(title);
    m_header->setCheckable(false);
    m_header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_header->setFixedHeight(22);
    m_header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_header->setArrowType(Qt::DownArrow);
    m_header->setStyleSheet(R"(
        QToolButton {
            background: #1c2128;
            color: #8b949e;
            border: none;
            border-top: 1px solid #21262d;
            border-bottom: 1px solid #21262d;
            font-size: 11px;
            font-weight: 600;
            letter-spacing: 1px;
            text-align: left;
            padding-left: 4px;
        }
        QToolButton:hover { background: #21262d; color: #c9d1d9; }
    )");
    outerLay->addWidget(m_header);

    m_body = new QWidget(this);
    m_body->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_bodyLayout = new QVBoxLayout(m_body);
    m_bodyLayout->setContentsMargins(0, 0, 0, 0);
    m_bodyLayout->setSpacing(0);
    outerLay->addWidget(m_body);

    connect(m_header, &QToolButton::clicked, this, &SidebarSection::toggle);

    bool expanded = m_settings->value(
        QStringLiteral("sidebar/expanded/") + key, true).toBool();
    m_body->setVisible(expanded);
    updateChevron();
}

void SidebarSection::addItem(SidebarItem* item)
{
    m_items << item;
    m_bodyLayout->addWidget(item);
}

void SidebarSection::clearItems()
{
    for (auto* w : m_items) {
        m_bodyLayout->removeWidget(w);
        w->deleteLater();
    }
    m_items.clear();
}

bool SidebarSection::isExpanded() const { return m_body->isVisible(); }

void SidebarSection::toggle()
{
    bool nowVisible = !m_body->isVisible();
    m_body->setVisible(nowVisible);
    m_settings->setValue(QStringLiteral("sidebar/expanded/") + m_key, nowVisible);
    updateChevron();
    if (parentWidget()) parentWidget()->adjustSize();
}

void SidebarSection::updateChevron()
{
    m_header->setArrowType(m_body->isVisible() ? Qt::DownArrow : Qt::RightArrow);
}

// ═══════════════════════════════════════════════════════════════════════════
// Sidebar
// ═══════════════════════════════════════════════════════════════════════════

Sidebar::Sidebar(Database* db, QWidget* parent)
    : QWidget(parent), m_db(db), m_settings("snipQ", "snipQ")
{
    setObjectName("Sidebar");

    auto* outerLay = new QVBoxLayout(this);
    outerLay->setContentsMargins(0, 0, 0, 0);
    outerLay->setSpacing(0);

    auto* titleLabel = new QLabel("  \u2b21  snipQ", this);
    titleLabel->setStyleSheet(
        "font-size:14px; font-weight:700; color:#58a6ff;"
        "padding:14px 12px 10px 12px; letter-spacing:1px;"
        "background:#161b22; border-bottom:1px solid #21262d;");
    outerLay->addWidget(titleLabel);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { background:transparent; border:none; }");

    auto* container = new QWidget(scroll);
    container->setObjectName("SidebarContainer");
    container->setStyleSheet("QWidget#SidebarContainer { background:#161b22; }");
    m_layout = new QVBoxLayout(container);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);
    m_layout->addStretch(1);

    scroll->setWidget(container);
    outerLay->addWidget(scroll, 1);

    build();
    // No emit here — MainWindow calls onSidebarSelection(defSel) explicitly
    // after all signals are connected, so the initial load is handled there.
}

// ─── Public ──────────────────────────────────────────────────────────────────

void Sidebar::refresh()
{
    if (m_selected) {
        m_selType     = m_selected->itemType();
        m_selFolderId = m_selected->payload().toInt();
        m_selTag      = m_selected->payload().toString();
    }

    build();

    auto trySelect = [&](SidebarSection* sec) {
        if (!sec) return;
        for (auto* item : sec->items()) {
            QString t = item->itemType();
            bool match = false;
            if (t == m_selType && t != "folder" && t != "tag") match = true;
            if (t == "folder" && m_selType == "folder" &&
                item->payload().toInt() == m_selFolderId)          match = true;
            if (t == "tag" && m_selType == "tag" &&
                item->payload().toString() == m_selTag)            match = true;
            if (match) { selectItem(item); return; }
        }
    };
    trySelect(m_libSection);
    trySelect(m_foldSection);
    trySelect(m_tagSection);
}

// ─── Private ─────────────────────────────────────────────────────────────────

static SidebarItem* makeItem(const QString& icon,
                             const QString& label,
                             const QString& type)
{
    return new SidebarItem(icon + "  " + label, type, nullptr);
}

void Sidebar::build()
{
    auto removeSection = [&](SidebarSection*& sec) {
        if (sec) {
            m_layout->removeWidget(sec);
            sec->deleteLater();
            sec = nullptr;
        }
    };
    removeSection(m_libSection);
    removeSection(m_foldSection);
    removeSection(m_tagSection);
    m_selected = nullptr;

    int insertPos = m_layout->count() - 1; // before stretch

    // ── LIBRARY ──────────────────────────────────────────────
    m_libSection = new SidebarSection("LIBRARY", "lib", &m_settings, this);

    auto* favItem = makeItem("\u2605", "Favourites", "lib_fav");
    favItem->setLabelColor("#e3b341");
    connect(favItem, &SidebarItem::clicked,
            this, [this](SidebarItem* i){ onItemClicked(i); });
    m_libSection->addItem(favItem);

    auto* allItem = makeItem("\u25c8", "All Snippets", "lib_all");
    connect(allItem, &SidebarItem::clicked,
            this, [this](SidebarItem* i){ onItemClicked(i); });
    m_libSection->addItem(allItem);

    auto* binItem = makeItem("\u2297", "Bin", "bin");
    binItem->setLabelColor("#6e7681");
    connect(binItem, &SidebarItem::clicked,
            this, [this](SidebarItem* i){ onItemClicked(i); });
    connect(binItem, &SidebarItem::contextMenuRequested,
            this, [this](SidebarItem* i, const QPoint& p){
                onItemContextMenu(i, p); });
    m_libSection->addItem(binItem);

    m_layout->insertWidget(insertPos++, m_libSection);

    // ── FOLDERS ───────────────────────────────────────────────
    m_foldSection = new SidebarSection("FOLDERS", "folders", &m_settings, this);

    for (const Folder& f : m_db->allFolders()) {
        auto* item = makeItem("\u25b8", f.name, "folder");
        item->setPayload(f.id);
        connect(item, &SidebarItem::clicked,
                this, [this](SidebarItem* i){ onItemClicked(i); });
        connect(item, &SidebarItem::contextMenuRequested,
                this, [this](SidebarItem* i, const QPoint& p){
                    onItemContextMenu(i, p); });
        m_foldSection->addItem(item);
    }
    m_layout->insertWidget(insertPos++, m_foldSection);

    // ── TAGS ──────────────────────────────────────────────────
    m_tagSection = new SidebarSection("TAGS", "tags", &m_settings, this);

    for (const QString& tag : m_db->allTags()) {
        auto* item = makeItem("#", tag, "tag");
        item->setPayload(tag);
        item->setLabelColor("#58a6ff");
        connect(item, &SidebarItem::clicked,
                this, [this](SidebarItem* i){ onItemClicked(i); });
        m_tagSection->addItem(item);
    }
    m_layout->insertWidget(insertPos++, m_tagSection);

    // If no prior selection exists, highlight All Snippets visually only
    // (MainWindow drives the actual load via onSidebarSelection)
    if (m_selType.isEmpty())
        selectItem(allItem);
}

void Sidebar::selectItem(SidebarItem* item)
{
    if (m_selected && m_selected != item)
        m_selected->setSelected(false);
    m_selected = item;
    if (item) item->setSelected(true);
}

void Sidebar::onItemClicked(SidebarItem* item)
{
    selectItem(item);

    SidebarSelection sel;
    QString t = item->itemType();
    if      (t == "lib_all") sel.type = SidebarSelection::AllSnippets;
    else if (t == "lib_fav") sel.type = SidebarSelection::Favourites;
    else if (t == "bin")     sel.type = SidebarSelection::Bin;
    else if (t == "folder") {
        sel.type     = SidebarSelection::Folder;
        sel.folderId = item->payload().toInt();
    } else if (t == "tag") {
        sel.type = SidebarSelection::Tag;
        sel.tag  = item->payload().toString();
    }
    emit selectionChanged(sel);
}

void Sidebar::onItemContextMenu(SidebarItem* item, const QPoint& globalPos)
{
    if (item->itemType() == "bin") {
        QMenu menu(this);
        auto* actEmpty = menu.addAction("Empty Trash\u2026");
        actEmpty->setIcon(QIcon());
        auto* chosen = menu.exec(globalPos);
        if (chosen == actEmpty) emit emptyTrashRequested();
        return;
    }

    if (item->itemType() != "folder") return;
    int fid = item->payload().toInt();

    QMenu menu(this);
    auto* actRename = menu.addAction("Rename Folder");
    auto* actDelete = menu.addAction("Delete Folder");
    menu.addSeparator();
    auto* actNew    = menu.addAction("New Subfolder");

    auto* chosen = menu.exec(globalPos);
    if (!chosen) return;
    if (chosen == actRename) emit renameFolderRequested(fid);
    if (chosen == actDelete) emit deleteFolderRequested(fid);
    if (chosen == actNew)    emit newFolderRequested(fid);
}
