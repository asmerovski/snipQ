#include "importexport.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

ImportExport::ImportExport(Database* db, QObject* parent)
    : QObject(parent), m_db(db) {}

bool ImportExport::exportToFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(QJsonDocument(m_db->exportToJson()).toJson(QJsonDocument::Indented));
    return true;
}

ImportResult ImportExport::importFromFile(const QString& path, bool skipDuplicates) {
    ImportResult fail;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return fail;
    auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return fail;
    return m_db->importFromJson(doc.object(), skipDuplicates);
}

ImportResult ImportExport::previewFile(const QString& path) {
    ImportResult fail;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return fail;
    auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return fail;
    return m_db->previewImport(doc.object());
}
