#pragma once
#include <QObject>
#include <QString>
#include "database.h"

class ImportExport : public QObject {
    Q_OBJECT
public:
    explicit ImportExport(Database* db, QObject* parent = nullptr);

    bool         exportToFile(const QString& path);
    // Returns result with counts; skipDuplicates and truncateNames control behaviour
    ImportResult importFromFile(const QString& path,
                                bool skipDuplicates = true,
                                bool truncateNames  = false);
    // Inspect the file without changing the DB
    ImportResult previewFile(const QString& path);

private:
    Database* m_db;
};
