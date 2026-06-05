#pragma once
#include <QString>

struct Folder {
    int     id       = -1;
    QString name;
    int     parentId = -1;   // -1 = root

    bool isValid() const { return id >= 0; }
};
