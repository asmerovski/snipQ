#pragma once
#include <QDialog>
#include <QLineEdit>

class NewFolderDialog : public QDialog {
    Q_OBJECT
public:
    explicit NewFolderDialog(const QString& title = "New Folder",
                             const QString& initial = {},
                             QWidget* parent = nullptr);
    QString folderName() const;

private:
    QLineEdit* m_nameEdit;
};
