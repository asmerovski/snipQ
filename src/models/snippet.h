#pragma once
#include <QString>
#include <QStringList>
#include <QDateTime>

struct SnippetTab {
    QString name;
    QString content;
    QString language;
};

struct Snippet {
    int         id         = -1;
    QString     name;
    QString     description;
    QStringList tags;
    int         folderId   = -1;
    bool        isFavorite = false;
    bool        isDeleted  = false;
    QDateTime   createdAt;
    QDateTime   updatedAt;
    QList<SnippetTab> tabs;   // each snippet can have multiple code tabs

    bool isValid() const { return id >= 0; }
};
