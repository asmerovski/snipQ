#include "importexport.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>

ImportExport::ImportExport(Database* db, QObject* parent)
    : QObject(parent), m_db(db) {}

bool ImportExport::exportToFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    QJsonObject root = m_db->exportToJson();
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

bool ImportExport::importFromFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return false;
    return m_db->importFromJson(doc.object());
}
