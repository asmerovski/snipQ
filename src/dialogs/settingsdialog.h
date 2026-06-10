#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QSpinBox>
#include <QFontComboBox>
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
    void onFontFamilyChanged(const QFont& font);
    void onFontSizeChanged(int size);

private:
    Database*     m_db;
    QLineEdit*    m_storagePath;
    QFontComboBox* m_fontCombo;
    QSpinBox*     m_fontSizeSpin;
    QLabel*       m_fontPreview;
};
