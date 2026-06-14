#pragma once
#include <QString>

// Formats/aligns code for a given language.
// Uses clang-format for C/C++/C#/Java/JS/TS/CSS/JSON/ObjC when available,
// falls back to a built-in indent-normaliser for everything else.
class CodeFormatter {
public:
    // Returns formatted code, or original if formatting fails/unsupported.
    static QString format(const QString& code, const QString& language);

    // Returns true if clang-format is available on PATH
    static bool clangFormatAvailable();

private:
    static QString clangFormat(const QString& code, const QString& style);
    static QString normaliseIndent(const QString& code, const QString& language);
    static QString formatJson(const QString& code);
    static QString formatXml(const QString& code);
    static QString formatSql(const QString& code);
    static QString formatYaml(const QString& code);
};
