#pragma once
#include <QObject>
#include <QString>
#include "database.h"

class ImportExport : public QObject {
    Q_OBJECT
public:
    explicit ImportExport(Database* db, QObject* parent = nullptr);

    bool exportToFile(const QString& path);
    bool importFromFile(const QString& path);

private:
    Database* m_db;
};
