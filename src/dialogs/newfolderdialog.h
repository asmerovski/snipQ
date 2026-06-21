#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QLabel>

class NewFolderDialog : public QDialog {
    Q_OBJECT
public:
    explicit NewFolderDialog(const QString& title = "New Folder",
                             const QString& initial = {},
                             QWidget* parent = nullptr);
    QString folderName() const;

private:
    QLineEdit* m_nameEdit;
    QLabel*    m_counter;
};
