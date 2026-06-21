#include <functional>
#include "mainwindow.h"
#include "theme.h"
#include "newfolderdialog.h"
#include "newsnippetdialog.h"
#include "settingsdialog.h"

#include <QMenuBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QApplication>
#include <QPushButton>
#include <QLabel>
#include <QCloseEvent>
#include "limits.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      m_settings("snipQ", "snipQ") {

    setWindowTitle("snipQ");
    setMinimumSize(900, 600);
    resize(1200, 750);

    // ── Database ─────────────────────────────────────────────
    m_db   = new Database(this);
    m_io   = new ImportExport(m_db, this);

    loadSettings();

    // ── Central layout ───────────────────────────────────────
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setHandleWidth(1);

    m_sidebar     = new Sidebar(m_db, this);
    m_snippetList = new SnippetList(m_db, this);
    m_editor      = new SnippetEditor(m_db, this);

    m_splitter->addWidget(m_sidebar);
    m_splitter->addWidget(m_snippetList);
    m_splitter->addWidget(m_editor);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 0);
    m_splitter->setStretchFactor(2, 1);
    m_splitter->setSizes({220, 260, 600});

    setCentralWidget(m_splitter);

    setupMenuBar();
    setupStatusBar();
    restoreWindowState();  // safe here — m_splitter and all widgets exist

    // ── Wire signals ─────────────────────────────────────────
    connect(m_sidebar, &Sidebar::selectionChanged,
            this,      &MainWindow::onSidebarSelection);
    connect(m_sidebar, &Sidebar::newFolderRequested,
            this,      &MainWindow::onNewFolder);
    connect(m_sidebar, &Sidebar::renameFolderRequested,
            this,      &MainWindow::onRenameFolder);
    connect(m_sidebar, &Sidebar::deleteFolderRequested,
            this,      &MainWindow::onDeleteFolder);
    connect(m_sidebar, &Sidebar::emptyTrashRequested,
            this,      &MainWindow::onEmptyTrash);

    connect(m_snippetList, &SnippetList::snippetSelected,
            this,          &MainWindow::onSnippetSelected);
    connect(m_snippetList, &SnippetList::newSnippetRequested,
            this,          &MainWindow::onNewSnippet);
    connect(m_snippetList, &SnippetList::deleteSnippetRequested,
            this,          &MainWindow::onDeleteSnippet);
    connect(m_snippetList, &SnippetList::restoreSnippetRequested,
            this,          &MainWindow::onRestoreSnippet);
    connect(m_snippetList, &SnippetList::toggleFavoriteRequested,
            this,          &MainWindow::onToggleFavorite);

    connect(m_editor, &SnippetEditor::snippetModified,
            this,     &MainWindow::onDataChanged);

    connect(m_db, &Database::dataChanged,
            this, &MainWindow::onDataChanged);

    // ── Restore last session ──────────────────────────────────
    // Disconnect dataChanged during startup so queued refreshes from
    // database open don't race against the restore. Reconnect after.
    disconnect(m_db, &Database::dataChanged, this, &MainWindow::onDataChanged);

    int lastId = m_settings.value("session/lastSnippetId", -1).toInt();
    if (lastId >= 0) {
        Snippet s = m_db->snippetById(lastId);
        if (s.isValid() && !s.isDeleted)
            m_snippetList->pinSnippet(lastId);
        else
            lastId = -1;
    }

    // Initial list load — populateList will honour the pinned ID
    SidebarSelection defSel;
    defSel.type = SidebarSelection::AllSnippets;
    onSidebarSelection(defSel);

    // Open the snippet in the editor
    if (lastId >= 0) {
        Snippet s = m_db->snippetById(lastId);
        m_editor->loadSnippet(lastId);
        statusBar()->showMessage(
            QStringLiteral("Restored: %1").arg(s.name), 2000);
    }

    // Reconnect — all subsequent data changes update normally
    connect(m_db, &Database::dataChanged, this, &MainWindow::onDataChanged);
}

MainWindow::~MainWindow() {
    saveSettings();
    m_db->close();
}

static QAction* makeAction(const QString& text, const QKeySequence& sc,
                             QObject* recv, std::function<void()> fn, QObject* parent) {
    auto* a = new QAction(text, parent);
    if (!sc.isEmpty()) a->setShortcut(sc);
    QObject::connect(a, &QAction::triggered, recv, fn);
    return a;
}

void MainWindow::setupMenuBar() {
    auto* mb = menuBar();

    // File
    auto* fileMenu = mb->addMenu("&File");
    fileMenu->addAction(makeAction("New Snippet",  QKeySequence::New,         this, [this]{ onNewSnippet(); }, this));
    fileMenu->addSeparator();
    fileMenu->addAction(makeAction("Export JSON…", QKeySequence("Ctrl+E"), this, [this]{ onExport(); },    this));
    fileMenu->addAction(makeAction("Import JSON…", QKeySequence("Ctrl+I"), this, [this]{ onImport(); },    this));
    fileMenu->addSeparator();
    fileMenu->addAction(makeAction("Settings",     QKeySequence::Preferences, this, [this]{ onSettings(); },  this));
    fileMenu->addSeparator();
    fileMenu->addAction(makeAction("Quit",         QKeySequence::Quit,        qApp, []{ QApplication::quit(); }, this));


    // Help
    auto* helpMenu = mb->addMenu("&Help");
    helpMenu->addAction(makeAction("About snipQ", {}, this, [this]{
        QMessageBox::about(this, "About snipQ",
            "<b>snipQ v1.0.0</b><br>"
            "A cross-platform code snippet manager.<br><br>"
            "Author: AsmerM<br><br>"
            "Built with Qt6 · SQLite · C++17");
    }, this));
}

void MainWindow::setupStatusBar() {
    statusBar()->showMessage("Ready");
}

void MainWindow::loadSettings() {
    // Phase 1: open DB only — called early in constructor before widgets exist.
    // Never touch m_splitter or any widget here.
    QString dbPath = m_settings.value("storage/path", defaultDbPath()).toString();
    QDir().mkpath(QFileInfo(dbPath).absolutePath());
    if (!m_db->open(dbPath)) {
        QMessageBox::critical(this, "Database Error",
                              "Could not open or create the database at:\n" + dbPath);
    }
}

void MainWindow::restoreWindowState() {
    // Phase 2: geometry + splitter restore — called after all widgets are built.
    if (m_settings.contains("window/geometry"))
        restoreGeometry(m_settings.value("window/geometry").toByteArray());
    if (m_settings.contains("splitter/state"))
        m_splitter->restoreState(m_settings.value("splitter/state").toByteArray());
}

void MainWindow::saveSettings() {
    m_settings.setValue("storage/path", m_db->currentPath());
    m_settings.setValue("window/geometry", saveGeometry());
    m_settings.setValue("splitter/state", m_splitter->saveState());
}

QString MainWindow::defaultDbPath() const {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return dir + "/snipQ.db";
}

void MainWindow::closeEvent(QCloseEvent* event) {
    saveSettings();
    event->accept();
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void MainWindow::onNewSnippet() {
    int defFolder = -1;
    if (m_currentSel.type == SidebarSelection::Folder)
        defFolder = m_currentSel.folderId;

    NewSnippetDialog dlg(m_db->allFolders(), defFolder, this);
    if (dlg.exec() != QDialog::Accepted) return;

    Snippet s;
    s.name     = dlg.name().isEmpty() ? "Untitled" : dlg.name();
    s.folderId = dlg.folderId();
    m_db->insertSnippet(s);

    m_snippetList->refresh();
    m_editor->loadSnippet(s.id);
    statusBar()->showMessage(QStringLiteral("Created snippet: %1").arg(s.name), 3000);
}

void MainWindow::onDeleteSnippet(int id) {
    bool permanent = (m_currentSel.type == SidebarSelection::Bin);
    if (permanent) {
        if (QMessageBox::question(this, "Delete", "Permanently delete this snippet?")
            != QMessageBox::Yes) return;
        m_db->deleteSnippet(id, true);
    } else {
        m_db->deleteSnippet(id, false);
    }
    m_editor->clearEditor();
    m_snippetList->refresh();
}

void MainWindow::onRestoreSnippet(int id) {
    m_db->restoreSnippet(id);
    m_snippetList->refresh();
}

void MainWindow::onToggleFavorite(int id) {
    m_db->toggleFavorite(id);
    m_snippetList->refresh();
}

void MainWindow::onSidebarSelection(const SidebarSelection& sel) {
    m_currentSel = sel;
    m_snippetList->loadFor(sel);
}

void MainWindow::onSnippetSelected(int id) {
    m_editor->loadSnippet(id);
    m_settings.setValue("session/lastSnippetId", id);
    m_settings.sync();  // flush immediately — survives crashes
}

void MainWindow::onNewFolder(int parentId) {
    NewFolderDialog dlg("New Folder", {}, this);
    if (dlg.exec() != QDialog::Accepted) return;
    Folder f;
    f.name     = dlg.folderName();
    f.parentId = parentId;
    m_db->insertFolder(f);
    m_sidebar->refresh();
    statusBar()->showMessage(QStringLiteral("Folder created: %1").arg(f.name), 2000);
}

void MainWindow::onRenameFolder(int id) {
    Folder f = m_db->folderById(id);
    if (!f.isValid()) return;
    NewFolderDialog dlg("Rename Folder", f.name, this);
    if (dlg.exec() != QDialog::Accepted) return;
    f.name = dlg.folderName();
    m_db->updateFolder(f);
    m_sidebar->refresh();
}

void MainWindow::onEmptyTrash() {
    int count = m_db->deletedSnippets().size();
    if (count == 0) {
        statusBar()->showMessage("Trash is already empty.", 2000);
        return;
    }
    if (QMessageBox::question(this, "Empty Trash",
            QStringLiteral("Permanently delete all %1 snippet(s) in the Bin?\n"
                           "This cannot be undone.").arg(count))
        != QMessageBox::Yes) return;

    m_db->emptyTrash();
    m_editor->clearEditor();
    m_snippetList->refresh();
    statusBar()->showMessage(
        QStringLiteral("Trash emptied: %1 snippet(s) deleted.").arg(count), 3000);
}

void MainWindow::onDeleteFolder(int id) {
    if (QMessageBox::question(this, "Delete Folder",
            "Delete this folder? Snippets inside will be moved to root.")
        != QMessageBox::Yes) return;
    m_db->deleteFolder(id);
    m_sidebar->refresh();
    SidebarSelection s;
    s.type = SidebarSelection::AllSnippets;
    onSidebarSelection(s);
}

void MainWindow::onExport() {
    QString path = QFileDialog::getSaveFileName(this, "Export Snippets",
        QDir::homePath() + "/snipQ_export.json",
        "JSON Files (*.json)");
    if (path.isEmpty()) return;
    if (m_io->exportToFile(path)) {
        statusBar()->showMessage("Exported to " + path, 4000);
    } else {
        QMessageBox::warning(this, "Export Failed", "Could not write export file.");
    }
}

void MainWindow::onImport() {
    QString path = QFileDialog::getOpenFileName(this, "Import Snippets",
        QDir::homePath(), "JSON Files (*.json)");
    if (path.isEmpty()) return;

    // Preview the file first — no DB changes yet
    ImportResult preview = m_io->previewFile(path);
    if (!preview.success && preview.snippetsTotal == 0 && preview.foldersTotal == 0) {
        QMessageBox::warning(this, "Import Failed",
            "Could not parse the JSON file or it contains no data.");
        return;
    }

    // ── Over-length name detection ────────────────────────────────────────
    bool hasTooLong = !preview.tooLongSnippetNames.isEmpty()
                   || !preview.tooLongFolderNames.isEmpty()
                   || !preview.tooLongTagNames.isEmpty();

    if (hasTooLong) {
        QString warnMsg = QStringLiteral(
            "<b>Some names in this file exceed the %1-character limit:</b><br><br>")
            .arg(NAME_MAX_LEN);

        auto appendList = [&](const QString& label, const QStringList& names) {
            if (names.isEmpty()) return;
            warnMsg += QStringLiteral("<b style='color:#f0883e'>%1</b><br>").arg(label);
            QStringList shown = names.mid(0, 5);
            for (auto& n : shown)
                warnMsg += QStringLiteral("&nbsp;&nbsp;• %1 <span style='color:#8b949e'>"
                                          "(%2 chars)</span><br>").arg(n.toHtmlEscaped()).arg(n.length());
            if (names.size() > 5)
                warnMsg += QStringLiteral("&nbsp;&nbsp;… and %1 more<br>").arg(names.size() - 5);
            warnMsg += "<br>";
        };

        appendList(QStringLiteral("Snippet names (%1):").arg(preview.tooLongSnippetNames.size()),
                   preview.tooLongSnippetNames);
        appendList(QStringLiteral("Folder names (%1):").arg(preview.tooLongFolderNames.size()),
                   preview.tooLongFolderNames);
        appendList(QStringLiteral("Tags (%1):").arg(preview.tooLongTagNames.size()),
                   preview.tooLongTagNames);

        warnMsg += QStringLiteral(
            "Choose <b>Truncate &amp; Import</b> to cut names to %1 characters and proceed,<br>"
            "or <b>Cancel</b> to abort and fix the file manually.").arg(NAME_MAX_LEN);

        QMessageBox dlgWarn(this);
        dlgWarn.setWindowTitle("Names Too Long");
        dlgWarn.setTextFormat(Qt::RichText);
        dlgWarn.setText(warnMsg);
        dlgWarn.setIcon(QMessageBox::Warning);
        auto* btnTruncate = dlgWarn.addButton("Truncate & Import", QMessageBox::AcceptRole);
        auto* btnAbort    = dlgWarn.addButton("Cancel",            QMessageBox::RejectRole);
        dlgWarn.setDefaultButton(btnAbort);
        dlgWarn.exec();
        if (dlgWarn.clickedButton() != btnTruncate) return;
    }

    // ── Duplicate / summary dialog ────────────────────────────────────────
    bool hasDupes = (preview.snippetsSkipped > 0 || preview.foldersSkipped > 0);
    bool hasNew   = (preview.snippetsAdded   > 0 || preview.foldersAdded   > 0);

    QString msg;
    msg += QStringLiteral(
        "<b>Import summary</b> for:<br><code>%1</code><br><br>"
        "<table cellspacing='4'>"
        "<tr><td>Snippets in file:</td><td><b>%2</b></td></tr>"
        "<tr><td>New (will be added):</td><td style='color:#3fb950'><b>%3</b></td></tr>"
        "<tr><td>Duplicates (same name exists):</td><td style='color:#f0883e'><b>%4</b></td></tr>"
        "<tr><td>Folders in file:</td><td><b>%5</b></td></tr>"
        "<tr><td>New folders:</td><td style='color:#3fb950'><b>%6</b></td></tr>"
        "<tr><td>Duplicate folders:</td><td style='color:#f0883e'><b>%7</b></td></tr>"
        "</table>")
        .arg(QFileInfo(path).fileName())
        .arg(preview.snippetsTotal)
        .arg(preview.snippetsAdded)
        .arg(preview.snippetsSkipped)
        .arg(preview.foldersTotal)
        .arg(preview.foldersAdded)
        .arg(preview.foldersSkipped);

    if (hasTooLong)
        msg += QStringLiteral(
            "<br><span style='color:#f0883e'>⚠ Over-length names will be truncated to %1 chars.</span>")
            .arg(NAME_MAX_LEN);

    if (hasDupes && !preview.duplicateNames.isEmpty()) {
        QStringList shown = preview.duplicateNames.mid(0, 8);
        if (preview.duplicateNames.size() > 8)
            shown << QStringLiteral("… and %1 more")
                     .arg(preview.duplicateNames.size() - 8);
        msg += QStringLiteral("<br><br><b style='color:#f0883e'>⚠ Duplicate snippets:</b><br>")
             + shown.join("<br>");
    }

    if (hasDupes) {
        msg += QStringLiteral(
            "<br><br><b style='color:#f0883e'>Warning:</b> Importing duplicates "
            "will create multiple snippets with the same name, which can cause "
            "confusion. Choose <b>Skip Duplicates</b> to import only new items, "
            "or <b>Import All</b> to import everything regardless.");
    }

    if (!hasNew && !hasDupes && !hasTooLong) {
        QMessageBox::information(this, "Nothing to Import",
            "All snippets in this file already exist in your library.");
        return;
    }

    // Build button set based on what's present
    QMessageBox dlg(this);
    dlg.setWindowTitle("Import Snippets");
    dlg.setTextFormat(Qt::RichText);
    dlg.setText(msg);
    dlg.setIcon(hasDupes ? QMessageBox::Warning : QMessageBox::Question);

    QPushButton* btnSkip   = nullptr;
    QPushButton* btnAll    = nullptr;
    QPushButton* btnCancel = dlg.addButton("Cancel", QMessageBox::RejectRole);

    if (hasDupes && hasNew)
        btnSkip = dlg.addButton("Skip Duplicates", QMessageBox::AcceptRole);
    if (hasDupes)
        btnAll  = dlg.addButton("Import All", QMessageBox::DestructiveRole);
    if (!hasDupes)
        btnSkip = dlg.addButton("Import", QMessageBox::AcceptRole);

    dlg.setDefaultButton(btnSkip ? btnSkip : btnCancel);
    dlg.exec();

    QAbstractButton* clicked = dlg.clickedButton();
    if (clicked == btnCancel || clicked == nullptr) return;

    bool skipDuplicates = (clicked != btnAll);

    ImportResult result = m_io->importFromFile(path, skipDuplicates, hasTooLong);
    if (!result.success) {
        QMessageBox::warning(this, "Import Failed",
            "The import transaction failed. No changes were made.");
        return;
    }

    m_sidebar->refresh();
    m_snippetList->refresh();

    QString summary = QStringLiteral(
        "Import complete: %1 snippet(s) added, %2 skipped")
        .arg(result.snippetsAdded)
        .arg(result.snippetsSkipped);
    if (result.truncatedCount > 0)
        summary += QStringLiteral(", %1 name(s) truncated to %2 chars")
                   .arg(result.truncatedCount).arg(NAME_MAX_LEN);
    summary += ".";
    statusBar()->showMessage(summary, 6000);
}

void MainWindow::onSettings() {
    SettingsDialog dlg(m_db, this);
    connect(&dlg, &SettingsDialog::storagePathChanged, this, [this](const QString& p) {
        m_settings.setValue("storage/path", p);
        QMessageBox::information(this, "Storage Moved",
            "Please restart snipQ to apply the new storage location.");
    });
    dlg.exec();
}


void MainWindow::onDataChanged() {
    // Use QueuedConnection semantics via invokeMethod so this always runs
    // AFTER the current call stack fully unwinds. Without this, a save
    // triggered inside onCurrentRowChanged causes populateList to be called
    // re-entrantly while QListWidget is mid-selection → crash in QTextEngine.
    QMetaObject::invokeMethod(this, [this]() {
        m_sidebar->refresh();
        m_snippetList->refresh();
    }, Qt::QueuedConnection);
}
