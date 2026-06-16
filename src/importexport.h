#pragma once
#include <QObject>
#include <QString>
#include "database.h"

class ImportExport : public QObject {
    Q_OBJECT
public:
    explicit ImportExport(Database* db, QObject* parent = nullptr);

    bool         exportToFile(const QString& path);
    // Returns result with counts; skipDuplicates controls behaviour
    ImportResult importFromFile(const QString& path, bool skipDuplicates = true);
    // Inspect the file without changing the DB
    ImportResult previewFile(const QString& path);

private:
    Database* m_db;
};
