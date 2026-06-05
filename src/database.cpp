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

Database::~Database() {
    close();
}

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
    if (m_db.isOpen()) {
        m_db.close();
    }
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
            tags        TEXT DEFAULT '',
            folder_id   INTEGER DEFAULT -1,
            is_favorite INTEGER DEFAULT 0,
            is_deleted  INTEGER DEFAULT 0,
            created_at  TEXT NOT NULL,
            updated_at  TEXT NOT NULL
        )
    )");
    ok &= q.exec(R"(
        CREATE TABLE IF NOT EXISTS snippet_tabs (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            snippet_id  INTEGER NOT NULL REFERENCES snippets(id) ON DELETE CASCADE,
            tab_index   INTEGER NOT NULL DEFAULT 0,
            name        TEXT NOT NULL DEFAULT 'Fragment 1',
            content     TEXT DEFAULT '',
            language    TEXT DEFAULT 'plaintext'
        )
    )");

    if (!ok) {
        qWarning() << "Schema creation error:" << q.lastError().text();
    }
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
    // tags stored comma-separated; use LIKE for simplicity
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
        INSERT INTO snippets (name,description,tags,folder_id,is_favorite,is_deleted,created_at,updated_at)
        VALUES (:name,:desc,:tags,:fid,:fav,:del,:cat,:uat)
    )");
    QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    q.bindValue(":name", snippet.name.isEmpty() ? "Untitled" : snippet.name);
    q.bindValue(":desc", snippet.description);
    q.bindValue(":tags", snippet.tags.join(","));
    q.bindValue(":fid",  snippet.folderId);
    q.bindValue(":fav",  snippet.isFavorite ? 1 : 0);
    q.bindValue(":del",  snippet.isDeleted  ? 1 : 0);
    q.bindValue(":cat",  now);
    q.bindValue(":uat",  now);
    if (!q.exec()) { qWarning() << q.lastError(); return false; }
    snippet.id = q.lastInsertId().toInt();
    if (snippet.tabs.isEmpty()) {
        snippet.tabs.append({"Fragment 1", "", "plaintext"});
    }
    saveSnippetTabs(snippet.id, snippet.tabs);
    emit dataChanged();
    return true;
}

bool Database::updateSnippet(const Snippet& snippet) {
    QSqlQuery q(m_db);
    q.prepare(R"(
        UPDATE snippets SET name=:name,description=:desc,tags=:tags,folder_id=:fid,
        is_favorite=:fav,is_deleted=:del,updated_at=:uat WHERE id=:id
    )");
    q.bindValue(":name", snippet.name);
    q.bindValue(":desc", snippet.description);
    q.bindValue(":tags", snippet.tags.join(","));
    q.bindValue(":fid",  snippet.folderId);
    q.bindValue(":fav",  snippet.isFavorite ? 1 : 0);
    q.bindValue(":del",  snippet.isDeleted  ? 1 : 0);
    q.bindValue(":uat",  QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    q.bindValue(":id",   snippet.id);
    if (!q.exec()) { qWarning() << q.lastError(); return false; }
    saveSnippetTabs(snippet.id, snippet.tabs);
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
    // Move snippets to root
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
        auto parts = q.value(0).toString().split(",", Qt::SkipEmptyParts);
        for (auto& p : parts) {
            QString t = p.trimmed();
            if (!t.isEmpty() && !all.contains(t))
                all << t;
        }
    }
    all.sort(Qt::CaseInsensitive);
    return all;
}

// ─── Import / Export ─────────────────────────────────────────────────────────

QJsonObject Database::exportToJson() {
    QJsonObject root;
    root["version"] = 1;
    root["exportedAt"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    QJsonArray foldersArr;
    for (auto& f : allFolders()) {
        QJsonObject fo;
        fo["id"]       = f.id;
        fo["name"]     = f.name;
        fo["parentId"] = f.parentId;
        foldersArr.append(fo);
    }
    root["folders"] = foldersArr;

    QJsonArray snippetsArr;
    for (auto& s : allSnippets(true)) {
        QJsonObject so;
        so["id"]          = s.id;
        so["name"]        = s.name;
        so["description"] = s.description;
        so["tags"]        = QJsonArray::fromStringList(s.tags);
        so["folderId"]    = s.folderId;
        so["isFavorite"]  = s.isFavorite;
        so["isDeleted"]   = s.isDeleted;
        so["createdAt"]   = s.createdAt.toString(Qt::ISODate);
        so["updatedAt"]   = s.updatedAt.toString(Qt::ISODate);
        QJsonArray tabsArr;
        for (auto& t : s.tabs) {
            QJsonObject to;
            to["name"]     = t.name;
            to["content"]  = t.content;
            to["language"] = t.language;
            tabsArr.append(to);
        }
        so["tabs"] = tabsArr;
        snippetsArr.append(so);
    }
    root["snippets"] = snippetsArr;
    return root;
}

bool Database::importFromJson(const QJsonObject& json) {
    m_db.transaction();
    // Import folders first
    QMap<int,int> folderIdMap; // old->new
    auto foldersArr = json["folders"].toArray();
    for (auto fv : foldersArr) {
        auto fo = fv.toObject();
        Folder f;
        f.name     = fo["name"].toString();
        f.parentId = fo["parentId"].toInt(-1);
        if (insertFolder(f)) folderIdMap[fo["id"].toInt()] = f.id;
    }
    // Import snippets
    auto snippetsArr = json["snippets"].toArray();
    for (auto sv : snippetsArr) {
        auto so = sv.toObject();
        Snippet s;
        s.name        = so["name"].toString();
        s.description = so["description"].toString();
        for (auto tv : so["tags"].toArray()) s.tags << tv.toString();
        int oldFid = so["folderId"].toInt(-1);
        s.folderId    = folderIdMap.value(oldFid, -1);
        s.isFavorite  = so["isFavorite"].toBool();
        s.isDeleted   = so["isDeleted"].toBool();
        for (auto tv : so["tabs"].toArray()) {
            auto to = tv.toObject();
            s.tabs.append({to["name"].toString(), to["content"].toString(), to["language"].toString()});
        }
        insertSnippet(s);
    }
    bool ok = m_db.commit();
    if (!ok) m_db.rollback();
    return ok;
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

bool Database::saveSnippetTabs(int snippetId, const QList<SnippetTab>& tabs) {
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM snippet_tabs WHERE snippet_id=:sid");
    q.bindValue(":sid", snippetId);
    q.exec();
    for (int i = 0; i < tabs.size(); ++i) {
        q.prepare("INSERT INTO snippet_tabs (snippet_id,tab_index,name,content,language) VALUES (:sid,:idx,:nm,:cnt,:lng)");
        q.bindValue(":sid", snippetId);
        q.bindValue(":idx", i);
        q.bindValue(":nm",  tabs[i].name);
        q.bindValue(":cnt", tabs[i].content);
        q.bindValue(":lng", tabs[i].language);
        q.exec();
    }
    return true;
}

QList<SnippetTab> Database::loadSnippetTabs(int snippetId) {
    QSqlQuery q(m_db);
    q.prepare("SELECT name,content,language FROM snippet_tabs WHERE snippet_id=:sid ORDER BY tab_index");
    q.bindValue(":sid", snippetId);
    q.exec();
    QList<SnippetTab> tabs;
    while (q.next()) {
        tabs.append({q.value(0).toString(), q.value(1).toString(), q.value(2).toString()});
    }
    if (tabs.isEmpty()) tabs.append({"Fragment 1", "", "plaintext"});
    return tabs;
}

Snippet Database::rowToSnippet(const QSqlQuery& q) {
    Snippet s;
    s.id          = q.value("id").toInt();
    s.name        = q.value("name").toString();
    s.description = q.value("description").toString();
    QString tags  = q.value("tags").toString();
    if (!tags.isEmpty()) s.tags = tags.split(",", Qt::SkipEmptyParts);
    s.folderId    = q.value("folder_id").toInt();
    s.isFavorite  = q.value("is_favorite").toBool();
    s.isDeleted   = q.value("is_deleted").toBool();
    s.createdAt   = QDateTime::fromString(q.value("created_at").toString(), Qt::ISODate);
    s.updatedAt   = QDateTime::fromString(q.value("updated_at").toString(), Qt::ISODate);
    s.tabs        = loadSnippetTabs(s.id);
    return s;
}
