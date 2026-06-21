#pragma once
#include <QMainWindow>
#include <QSplitter>
#include <QSettings>
#include "database.h"
#include "sidebar.h"
#include "snippetlist.h"
#include "snippeteditor.h"
#include "importexport.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onNewSnippet();
    void onDeleteSnippet(int id);
    void onRestoreSnippet(int id);
    void onToggleFavorite(int id);
    void onSidebarSelection(const SidebarSelection& sel);
    void onSnippetSelected(int id);
    void onNewFolder(int parentId);
    void onRenameFolder(int id);
    void onDeleteFolder(int id);
    void onEmptyTrash();
    void onExport();
    void onImport();
    void onSettings();
    void onDataChanged();

private:
    void setupMenuBar();
    void setupStatusBar();
    void loadSettings();
    void restoreWindowState();
    void saveSettings();
    QString defaultDbPath() const;

    Database*     m_db;
    ImportExport* m_io;

    Sidebar*       m_sidebar;
    SnippetList*   m_snippetList;
    SnippetEditor* m_editor;
    QSplitter*     m_splitter;

    SidebarSelection m_currentSel;
    QSettings        m_settings;
};
