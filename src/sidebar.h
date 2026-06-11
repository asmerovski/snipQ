#pragma once
#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QToolButton>
#include <QLabel>
#include <QSettings>
#include <QMap>
#include "database.h"

struct SidebarSelection {
    enum Type { AllSnippets, Favourites, Bin, Folder, Tag };
    Type    type     = AllSnippets;
    int     folderId = -1;
    QString tag;
};

// ── A single clickable row item ──────────────────────────────────────────────
class SidebarItem : public QWidget {
    Q_OBJECT
public:
    SidebarItem(const QString& label,
                const QString& itemType,
                QWidget* parent = nullptr);

    void setSelected(bool on);
    bool isSelected() const { return m_selected; }

    QString itemType() const { return m_type; }

    // extra data (folder id or tag name)
    void   setPayload(const QVariant& v) { m_payload = v; }
    QVariant payload() const             { return m_payload; }

signals:
    void clicked(SidebarItem* self);
    void contextMenuRequested(SidebarItem* self, const QPoint& globalPos);

protected:
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;
    void enterEvent(QEnterEvent* e) override;
    void leaveEvent(QEvent* e) override;
    void paintEvent(QPaintEvent* e) override;

private:
    QString  m_type;
    QVariant m_payload;
    bool     m_selected = false;
    bool     m_hovered  = false;
};

// ── Collapsible section ───────────────────────────────────────────────────────
class SidebarSection : public QWidget {
    Q_OBJECT
public:
    SidebarSection(const QString& title,
                   const QString& key,
                   QSettings*     settings,
                   QWidget*       parent = nullptr);

    void addItem(SidebarItem* item);
    void clearItems();
    QList<SidebarItem*> items() const { return m_items; }
    bool isExpanded() const;

private slots:
    void toggle();

private:
    void updateChevron();

    QString           m_key;
    QSettings*        m_settings;
    QToolButton*      m_header;
    QWidget*          m_body;
    QVBoxLayout*      m_bodyLayout;
    QList<SidebarItem*> m_items;
};

// ── Full sidebar ─────────────────────────────────────────────────────────────
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

private:
    void build();
    void selectItem(SidebarItem* item);
    void onItemClicked(SidebarItem* item);
    void onItemContextMenu(SidebarItem* item, const QPoint& globalPos);

    Database*         m_db;
    QSettings         m_settings;
    QVBoxLayout*      m_layout;   // inner layout inside scroll area
    SidebarItem*      m_selected = nullptr;

    SidebarSection*   m_libSection  = nullptr;
    SidebarSection*   m_foldSection = nullptr;
    SidebarSection*   m_tagSection  = nullptr;

    // Keep track for restore-selection after refresh
    QString           m_selType;
    int               m_selFolderId = -1;
    QString           m_selTag;
};
