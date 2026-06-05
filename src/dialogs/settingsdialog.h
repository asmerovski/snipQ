#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QCheckBox>
#include "database.h"

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(Database* db, QWidget* parent = nullptr);

signals:
    void storagePathChanged(const QString& newPath);

private slots:
    void onBrowseStorage();
    void onMoveStorage();

private:
    Database*  m_db;
    QLineEdit* m_storagePath;
};
