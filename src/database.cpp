#include "database.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDebug>
#include <QDateTime>

int Database::s_nextId = 0;

Database::Database(QObject* parent) : QObject(parent) {
    m_connectionId = s_nextId++;
}

Database::~Database() { close(); }

bool Database::open(const QString& path) {
    m_path = path;
    QString connName = QStringLiteral("snipQ_%1").arg(m_connectionId);
    m_db = QSqlDatabase::addDatabase("QSQLITE", connName);
    m_db.setDatabaseName(path);
    if (!m_db.open()) {
        qWarning() << "DB open failed:" << m_db.lastError().text();
        return false;
    }
    return createSchema();
}

void Database::close() {
    if (m_db.isOpen()) m_db.close();
}

bool Database::createSchema() {
    QSqlQuery q(m_db);
    q.exec("PRAGMA journal_mode=WAL;");
    q.exec("PRAGMA foreign_keys=ON;");

    bool ok = true;
    ok &= q.exec(R"(
        CREATE TABLE IF NOT EXISTS folders (
            id        INTEGER PRIMARY KEY AUTOINCREMENT,
            name      TEXT NOT NULL,
            parent_id INTEGER DEFAULT -1
        )
    )");
    ok &= q.exec(R"(
        CREATE TABLE IF NOT EXISTS snippets (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            name        TEXT NOT NULL DEFAULT 'Untitled',
            description TEXT DEFAULT '',
            content     TEXT DEFAULT '',
            language    TEXT DEFAULT 'plaintext',
            tags        TEXT DEFAULT '',
            folder_id   INTEGER DEFAULT -1,
            is_favorite INTEGER DEFAULT 0,
            is_deleted  INTEGER DEFAULT 0,
            created_at  TEXT NOT NULL,
            updated_at  TEXT NOT NULL
        )
    )");

    // Schema version guard — run migrations only when needed
    // Using PRAGMA user_version to track schema version
    int schemaVersion = 0;
    if (q.exec("PRAGMA user_version") && q.next())
        schemaVersion = q.value(0).toInt();

    if (schemaVersion < 1) {
        // v0→v1: add content + language columns (migration from tab-based schema)
        q.exec("ALTER TABLE snippets ADD COLUMN content TEXT DEFAULT ''");
        q.exec("ALTER TABLE snippets ADD COLUMN language TEXT DEFAULT 'plaintext'");
        q.exec("PRAGMA user_version = 1");
    }

    if (!ok) qWarning() << "Schema error:" << q.lastError().text();
    return ok;
}

// ─── Snippets ────────────────────────────────────────────────────────────────

QList<Snippet> Database::allSnippets(bool includeDeleted) {
    QSqlQuery q(m_db);
    QString sql = "SELECT * FROM snippets";
    if (!includeDeleted) sql += " WHERE is_deleted=0";
    sql += " ORDER BY updated_at DESC";
    q.exec(sql);
    QList<Snippet> list;
    while (q.next()) list << rowToSnippet(q);
    return list;
}

QList<Snippet> Database::snippetsByFolder(int folderId) {
    QSqlQuery q(m_db);
    q.prepare("SELECT * FROM snippets WHERE folder_id=:fid AND is_deleted=0 ORDER BY updated_at DESC");
    q.bindValue(":fid", folderId);
    q.exec();
    QList<Snippet> list;
    while (q.next()) list << rowToSnippet(q);
    return list;
}

QList<Snippet> Database::snippetsByTag(const QString& tag) {
    QSqlQuery q(m_db);
    q.prepare("SELECT * FROM snippets WHERE is_deleted=0 AND (',' || tags || ',') LIKE :pat ORDER BY updated_at DESC");
    q.bindValue(":pat", QStringLiteral("%,%1,%").arg(tag));
    q.exec();
    QList<Snippet> list;
    while (q.next()) list << rowToSnippet(q);
    return list;
}

QList<Snippet> Database::favoriteSnippets() {
    QSqlQuery q(m_db);
    q.exec("SELECT * FROM snippets WHERE is_favorite=1 AND is_deleted=0 ORDER BY updated_at DESC");
    QList<Snippet> list;
    while (q.next()) list << rowToSnippet(q);
    return list;
}

QList<Snippet> Database::deletedSnippets() {
    QSqlQuery q(m_db);
    q.exec("SELECT * FROM snippets WHERE is_deleted=1 ORDER BY updated_at DESC");
    QList<Snippet> list;
    while (q.next()) list << rowToSnippet(q);
    return list;
}

Snippet Database::snippetById(int id) {
    QSqlQuery q(m_db);
    q.prepare("SELECT * FROM snippets WHERE id=:id");
    q.bindValue(":id", id);
    q.exec();
    if (q.next()) return rowToSnippet(q);
    return Snippet{};
}

bool Database::insertSnippet(Snippet& snippet) {
    QSqlQuery q(m_db);
    q.prepare(R"(
        INSERT INTO snippets (name,description,content,language,tags,folder_id,is_favorite,is_deleted,created_at,updated_at)
        VALUES (:name,:desc,:content,:lang,:tags,:fid,:fav,:del,:cat,:uat)
    )");
    QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    q.bindValue(":name",    snippet.name.isEmpty() ? "Untitled" : snippet.name);
    q.bindValue(":desc",    snippet.description);
    q.bindValue(":content", snippet.content);
    q.bindValue(":lang",    snippet.language.isEmpty() ? "plaintext" : snippet.language);
    q.bindValue(":tags",    snippet.tags.join(","));
    q.bindValue(":fid",     snippet.folderId);
    q.bindValue(":fav",     snippet.isFavorite ? 1 : 0);
    q.bindValue(":del",     snippet.isDeleted  ? 1 : 0);
    q.bindValue(":cat",     now);
    q.bindValue(":uat",     now);
    if (!q.exec()) { qWarning() << q.lastError(); return false; }
    snippet.id = q.lastInsertId().toInt();
    emit dataChanged();
    return true;
}

bool Database::updateSnippet(const Snippet& snippet) {
    QSqlQuery q(m_db);
    q.prepare(R"(
        UPDATE snippets SET name=:name,description=:desc,content=:content,language=:lang,
        tags=:tags,folder_id=:fid,is_favorite=:fav,is_deleted=:del,updated_at=:uat WHERE id=:id
    )");
    q.bindValue(":name",    snippet.name);
    q.bindValue(":desc",    snippet.description);
    q.bindValue(":content", snippet.content);
    q.bindValue(":lang",    snippet.language);
    q.bindValue(":tags",    snippet.tags.join(","));
    q.bindValue(":fid",     snippet.folderId);
    q.bindValue(":fav",     snippet.isFavorite ? 1 : 0);
    q.bindValue(":del",     snippet.isDeleted  ? 1 : 0);
    q.bindValue(":uat",     QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    q.bindValue(":id",      snippet.id);
    if (!q.exec()) { qWarning() << q.lastError(); return false; }
    emit dataChanged();
    return true;
}

bool Database::deleteSnippet(int id, bool permanent) {
    QSqlQuery q(m_db);
    if (permanent) {
        q.prepare("DELETE FROM snippets WHERE id=:id");
    } else {
        q.prepare("UPDATE snippets SET is_deleted=1, updated_at=:uat WHERE id=:id");
        q.bindValue(":uat", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    }
    q.bindValue(":id", id);
    bool ok = q.exec();
    if (ok) emit dataChanged();
    return ok;
}

bool Database::restoreSnippet(int id) {
    QSqlQuery q(m_db);
    q.prepare("UPDATE snippets SET is_deleted=0, updated_at=:uat WHERE id=:id");
    q.bindValue(":uat", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    q.bindValue(":id", id);
    bool ok = q.exec();
    if (ok) emit dataChanged();
    return ok;
}

bool Database::toggleFavorite(int id) {
    QSqlQuery q(m_db);
    q.prepare("UPDATE snippets SET is_favorite=NOT is_favorite, updated_at=:uat WHERE id=:id");
    q.bindValue(":uat", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    q.bindValue(":id", id);
    bool ok = q.exec();
    if (ok) emit dataChanged();
    return ok;
}

// ─── Folders ─────────────────────────────────────────────────────────────────

QList<Folder> Database::allFolders() {
    QSqlQuery q(m_db);
    q.exec("SELECT id,name,parent_id FROM folders ORDER BY name");
    QList<Folder> list;
    while (q.next()) {
        Folder f;
        f.id       = q.value(0).toInt();
        f.name     = q.value(1).toString();
        f.parentId = q.value(2).toInt();
        list << f;
    }
    return list;
}

Folder Database::folderById(int id) {
    QSqlQuery q(m_db);
    q.prepare("SELECT id,name,parent_id FROM folders WHERE id=:id");
    q.bindValue(":id", id);
    q.exec();
    if (q.next()) {
        Folder f;
        f.id       = q.value(0).toInt();
        f.name     = q.value(1).toString();
        f.parentId = q.value(2).toInt();
        return f;
    }
    return Folder{};
}

bool Database::insertFolder(Folder& folder) {
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO folders (name,parent_id) VALUES (:name,:pid)");
    q.bindValue(":name", folder.name);
    q.bindValue(":pid",  folder.parentId);
    if (!q.exec()) { qWarning() << q.lastError(); return false; }
    folder.id = q.lastInsertId().toInt();
    emit dataChanged();
    return true;
}

bool Database::updateFolder(const Folder& folder) {
    QSqlQuery q(m_db);
    q.prepare("UPDATE folders SET name=:name,parent_id=:pid WHERE id=:id");
    q.bindValue(":name", folder.name);
    q.bindValue(":pid",  folder.parentId);
    q.bindValue(":id",   folder.id);
    bool ok = q.exec();
    if (ok) emit dataChanged();
    return ok;
}

bool Database::deleteFolder(int id) {
    QSqlQuery q(m_db);
    q.prepare("UPDATE snippets SET folder_id=-1 WHERE folder_id=:id");
    q.bindValue(":id", id);
    q.exec();
    q.prepare("DELETE FROM folders WHERE id=:id");
    q.bindValue(":id", id);
    bool ok = q.exec();
    if (ok) emit dataChanged();
    return ok;
}

// ─── Tags ────────────────────────────────────────────────────────────────────

QStringList Database::allTags() {
    QSqlQuery q(m_db);
    q.exec("SELECT tags FROM snippets WHERE is_deleted=0 AND tags!=''");
    QStringList all;
    while (q.next()) {
        for (auto& p : q.value(0).toString().split(",", Qt::SkipEmptyParts)) {
            QString t = p.trimmed();
            if (!t.isEmpty() && !all.contains(t)) all << t;
        }
    }
    all.sort(Qt::CaseInsensitive);
    return all;
}

// ─── Import / Export ─────────────────────────────────────────────────────────

QJsonObject Database::exportToJson() {
    QJsonObject root;
    root["version"]    = 2;
    root["exportedAt"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    QJsonArray foldersArr;
    for (auto& f : allFolders()) {
        QJsonObject fo;
        fo["id"] = f.id; fo["name"] = f.name; fo["parentId"] = f.parentId;
        foldersArr.append(fo);
    }
    root["folders"] = foldersArr;

    QJsonArray snippetsArr;
    for (auto& s : allSnippets(true)) {
        QJsonObject so;
        so["id"]          = s.id;
        so["name"]        = s.name;
        so["description"] = s.description;
        so["content"]     = s.content;
        so["language"]    = s.language;
        so["tags"]        = QJsonArray::fromStringList(s.tags);
        so["folderId"]    = s.folderId;
        so["isFavorite"]  = s.isFavorite;
        so["isDeleted"]   = s.isDeleted;
        so["createdAt"]   = s.createdAt.toString(Qt::ISODate);
        so["updatedAt"]   = s.updatedAt.toString(Qt::ISODate);
        snippetsArr.append(so);
    }
    root["snippets"] = snippetsArr;
    return root;
}

bool Database::importFromJson(const QJsonObject& json) {
    m_db.transaction();
    QMap<int,int> folderIdMap;
    for (auto fv : json["folders"].toArray()) {
        auto fo = fv.toObject();
        Folder f;
        f.name     = fo["name"].toString();
        f.parentId = fo["parentId"].toInt(-1);
        if (insertFolder(f)) folderIdMap[fo["id"].toInt()] = f.id;
    }
    for (auto sv : json["snippets"].toArray()) {
        auto so = sv.toObject();
        Snippet s;
        s.name        = so["name"].toString();
        s.description = so["description"].toString();
        s.content     = so["content"].toString();
        s.language    = so["language"].toString("plaintext");
        for (auto tv : so["tags"].toArray()) s.tags << tv.toString();
        s.folderId    = folderIdMap.value(so["folderId"].toInt(-1), -1);
        s.isFavorite  = so["isFavorite"].toBool();
        s.isDeleted   = so["isDeleted"].toBool();
        insertSnippet(s);
    }
    bool ok = m_db.commit();
    if (!ok) m_db.rollback();
    return ok;
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

Snippet Database::rowToSnippet(const QSqlQuery& q) {
    Snippet s;
    s.id          = q.value("id").toInt();
    s.name        = q.value("name").toString();
    s.description = q.value("description").toString();
    s.content     = q.value("content").toString();
    s.language    = q.value("language").toString();
    if (s.language.isEmpty()) s.language = QStringLiteral("plaintext");
    QString tags  = q.value("tags").toString();
    if (!tags.isEmpty()) s.tags = tags.split(",", Qt::SkipEmptyParts);
    s.folderId    = q.value("folder_id").toInt();
    s.isFavorite  = q.value("is_favorite").toBool();
    s.isDeleted   = q.value("is_deleted").toBool();
    s.createdAt   = QDateTime::fromString(q.value("created_at").toString(), Qt::ISODate);
    s.updatedAt   = QDateTime::fromString(q.value("updated_at").toString(), Qt::ISODate);
    return s;
}
