#pragma once
#include <QObject>
#include <QSqlDatabase>
#include <QList>
#include "snippet.h"
#include "folder.h"
#include "limits.h"

// Result of an import operation
struct ImportResult {
    bool    success        = false;
    int     snippetsTotal  = 0;  // in the JSON file
    int     snippetsAdded  = 0;  // actually inserted
    int     snippetsSkipped= 0;  // skipped as duplicates
    int     foldersTotal   = 0;
    int     foldersAdded   = 0;
    int     foldersSkipped = 0;
    QStringList duplicateNames;   // names of skipped snippets

    // Names that exceed NAME_MAX_LEN / TAG_MAX_LEN (detected during preview)
    QStringList tooLongSnippetNames;
    QStringList tooLongFolderNames;
    QStringList tooLongTagNames;
    int         truncatedCount = 0;  // items whose names were truncated during import
};

class Database : public QObject {
    Q_OBJECT

public:
    explicit Database(QObject* parent = nullptr);
    ~Database();

    bool open(const QString& path);
    void close();
    QString currentPath() const { return m_path; }

    // ── Snippets ──
    QList<Snippet> allSnippets(bool includeDeleted = false);
    QList<Snippet> snippetsByFolder(int folderId);
    QList<Snippet> snippetsByTag(const QString& tag);
    QList<Snippet> favoriteSnippets();
    QList<Snippet> deletedSnippets();
    Snippet        snippetById(int id);

    bool insertSnippet(Snippet& snippet);
    bool updateSnippet(const Snippet& snippet);
    bool deleteSnippet(int id, bool permanent = false);
    bool restoreSnippet(int id);
    bool emptyTrash();
    bool toggleFavorite(int id);

    // ── Folders ──
    QList<Folder> allFolders();
    Folder        folderById(int id);
    bool insertFolder(Folder& folder);
    bool updateFolder(const Folder& folder);
    bool deleteFolder(int id);

    // ── Tags ──
    QStringList allTags();

    // ── Import / Export ──
    QJsonObject  exportToJson();
    ImportResult importFromJson(const QJsonObject& json,
                                bool skipDuplicates = true,
                                bool truncateNames  = false);

    // Check duplicates and over-length names without importing
    ImportResult previewImport(const QJsonObject& json);

signals:
    void dataChanged();

private:
    bool createSchema();
    Snippet rowToSnippet(const QSqlQuery& q);
    bool snippetExistsByName(const QString& name) const;
    bool folderExistsByName(const QString& name) const;

    QSqlDatabase m_db;
    QString      m_path;
    int          m_connectionId = 0;
    static int   s_nextId;
};
