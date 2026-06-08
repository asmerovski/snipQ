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

bool ImportExport::importFromFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return false;
    return m_db->importFromJson(doc.object());
}
