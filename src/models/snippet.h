#pragma once
#include <QString>
#include <QStringList>
#include <QDateTime>

struct Snippet {
    int         id         = -1;
    QString     name;
    QString     description;
    QString     content;
    QString     language   = QStringLiteral("plaintext");
    QStringList tags;
    int         folderId   = -1;
    bool        isFavorite = false;
    bool        isDeleted  = false;
    QDateTime   createdAt;
    QDateTime   updatedAt;

    bool isValid() const { return id >= 0; }
};
