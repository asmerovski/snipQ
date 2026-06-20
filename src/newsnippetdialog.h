#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QList>
#include "folder.h"
#include <QLabel>

class NewSnippetDialog : public QDialog {
    Q_OBJECT
public:
    explicit NewSnippetDialog(const QList<Folder>& folders, int defaultFolder = -1,
                              QWidget* parent = nullptr);
    QString name() const;
    int     folderId() const;

private:
    QLineEdit* m_nameEdit;
    QLabel*    m_nameCounter;
    QLabel*    m_errorLabel;
    QComboBox* m_folderCombo;
    QList<int> m_folderIds;
};
