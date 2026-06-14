#include "codeformatter.h"
#include <QProcess>
#include <QTemporaryFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <QStack>

// ─── clang-format dispatch ───────────────────────────────────────────────────

bool CodeFormatter::clangFormatAvailable()
{
    static int cached = -1;
    if (cached >= 0) return cached;
    QProcess p;
    p.start("clang-format", {"--version"});
    cached = p.waitForFinished(3000) && p.exitCode() == 0 ? 1 : 0;
    return cached;
}

QString CodeFormatter::clangFormat(const QString& code, const QString& style)
{
    // Write to a temp file so clang-format gets the filename extension right
    QTemporaryFile tmp;
    if (!tmp.open()) return code;
    tmp.write(code.toUtf8());
    tmp.flush();

    QProcess p;
    p.start("clang-format", {
        QStringLiteral("--style=%1").arg(style),
        tmp.fileName()
    });
    if (!p.waitForFinished(10000) || p.exitCode() != 0)
        return code;

    return QString::fromUtf8(p.readAllStandardOutput());
}

// ─── Public entry point ───────────────────────────────────────────────────────

QString CodeFormatter::format(const QString& code, const QString& lang)
{
    if (code.trimmed().isEmpty()) return code;
    const QString l = lang.toLower();

    // ── clang-format languages ────────────────────────────────
    if (clangFormatAvailable()) {
        QString style;
        // Use .clang-format style strings — we pick sensible defaults per language
        const QString base = R"({BasedOnStyle: Google, IndentWidth: 4, ColumnLimit: 100, )";

        if (l == "c" || l == "cpp" || l == "c++" || l == "objectivec" || l == "objc")
            style = base + R"(SortIncludes: false})";
        else if (l == "csharp" || l == "c#")
            style = base + R"(BreakBeforeBraces: Allman})";
        else if (l == "java")
            style = base + R"(ColumnLimit: 120})";
        else if (l == "javascript" || l == "js" ||
                 l == "typescript" || l == "ts")
            style = base + R"(ColumnLimit: 100})";
        else if (l == "css")
            style = R"({BasedOnStyle: Google, IndentWidth: 2, ColumnLimit: 0})";
        else if (l == "proto")
            style = R"({BasedOnStyle: Google})";

        if (!style.isEmpty())
            return clangFormat(code, style);
    }

    // ── Built-in formatters ───────────────────────────────────
    if (l == "json")             return formatJson(code);
    if (l == "xml" || l == "html") return formatXml(code);
    if (l == "sql")              return formatSql(code);

    // ── Generic indent normaliser ─────────────────────────────
    return normaliseIndent(code, l);
}

// ─── JSON formatter ───────────────────────────────────────────────────────────

QString CodeFormatter::formatJson(const QString& code)
{
    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(code.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || doc.isNull())
        return code; // not valid JSON, return as-is
    return QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
}

// ─── XML / HTML formatter ─────────────────────────────────────────────────────

QString CodeFormatter::formatXml(const QString& code)
{
    // Simple indent-based XML pretty-printer (no parser dependency)
    QString out;
    QTextStream in(const_cast<QString*>(&code), QIODevice::ReadOnly);
    int depth = 0;
    const QString indent(4, ' ');

    // Split on tag boundaries
    QStringList tokens;
    QString remaining = code;
    static QRegularExpression tagRe(R"(<[^>]+>|[^<]+)");
    auto it = tagRe.globalMatch(code);
    while (it.hasNext()) {
        auto m = it.next();
        QString t = m.captured().trimmed();
        if (t.isEmpty()) continue;
        tokens << t;
    }

    for (const QString& tok : tokens) {
        if (tok.startsWith("</")) {
            // Closing tag
            depth = qMax(0, depth - 1);
            out += indent.repeated(depth) + tok + "\n";
        } else if (tok.startsWith("<") && !tok.startsWith("<?") &&
                   !tok.startsWith("<!") && !tok.endsWith("/>")) {
            // Opening tag
            out += indent.repeated(depth) + tok + "\n";
            depth++;
        } else if (tok.startsWith("<") && (tok.endsWith("/>") ||
                   tok.startsWith("<?") || tok.startsWith("<!"))) {
            // Self-closing or declaration
            out += indent.repeated(depth) + tok + "\n";
        } else {
            // Text content
            out += indent.repeated(depth) + tok + "\n";
        }
    }
    return out.trimmed();
}

// ─── SQL formatter ────────────────────────────────────────────────────────────

QString CodeFormatter::formatSql(const QString& code)
{
    // Keyword-aware SQL formatter: uppercase keywords, newlines on major clauses
    static const QStringList majorClauses = {
        "SELECT","FROM","WHERE","GROUP BY","HAVING","ORDER BY","LIMIT",
        "OFFSET","JOIN","INNER JOIN","LEFT JOIN","RIGHT JOIN","FULL JOIN",
        "LEFT OUTER JOIN","RIGHT OUTER JOIN","CROSS JOIN","ON",
        "INSERT INTO","VALUES","UPDATE","SET","DELETE FROM",
        "CREATE TABLE","ALTER TABLE","DROP TABLE","WITH","UNION","UNION ALL",
        "INTERSECT","EXCEPT","BEGIN","COMMIT","ROLLBACK"
    };
    static const QStringList keywords = {
        "SELECT","FROM","WHERE","AND","OR","NOT","IN","BETWEEN","LIKE","IS",
        "NULL","EXISTS","CASE","WHEN","THEN","ELSE","END","ORDER","BY","ASC",
        "DESC","GROUP","HAVING","LIMIT","OFFSET","JOIN","INNER","LEFT","RIGHT",
        "FULL","OUTER","CROSS","ON","UNION","ALL","INTERSECT","EXCEPT",
        "INSERT","INTO","VALUES","UPDATE","SET","DELETE","CREATE","TABLE",
        "VIEW","INDEX","DROP","ALTER","ADD","COLUMN","PRIMARY","KEY",
        "FOREIGN","REFERENCES","UNIQUE","DEFAULT","CHECK","CONSTRAINT",
        "BEGIN","COMMIT","ROLLBACK","TRANSACTION","WITH","AS","DISTINCT",
        "COUNT","SUM","AVG","MIN","MAX","COALESCE","CAST","CONVERT","TRIM",
        "UPPER","LOWER","CONCAT","NOW","ROUND","FLOOR","CEIL","ABS"
    };

    // Tokenise on whitespace and punctuation, preserving strings
    QString result;
    int     indent = 0;
    bool    inStr  = false;
    QChar   strQ;
    QString word;
    QString src = code.simplified();

    auto flushWord = [&]() {
        if (word.isEmpty()) return;
        QString up = word.toUpper();
        bool isKw = keywords.contains(up);
        bool isMajor = majorClauses.contains(up);

        if (isMajor) {
            if (!result.trimmed().isEmpty() && !result.endsWith('\n'))
                result += '\n';
            result += QString(indent * 4, ' ') + up + ' ';
        } else if (isKw) {
            result += up + ' ';
        } else {
            result += word + ' ';
        }
        word.clear();
    };

    for (int i = 0; i < src.length(); ++i) {
        QChar c = src[i];
        if (inStr) {
            word += c;
            if (c == strQ) inStr = false;
        } else if (c == '\'' || c == '"') {
            flushWord();
            inStr = true;
            strQ  = c;
            word  = c;
        } else if (c == '(') {
            flushWord();
            result += "(\n" + QString((indent + 1) * 4, ' ');
            indent++;
        } else if (c == ')') {
            flushWord();
            indent = qMax(0, indent - 1);
            result = result.trimmed() + '\n' + QString(indent * 4, ' ') + ") ";
        } else if (c == ',') {
            flushWord();
            result = result.trimmed() + ",\n" + QString(indent * 4, ' ');
        } else if (c == ';') {
            flushWord();
            result = result.trimmed() + ";\n";
        } else if (c.isSpace()) {
            flushWord();
        } else {
            word += c;
        }
    }
    flushWord();
    return result.trimmed();
}

// ─── Generic indent normaliser ────────────────────────────────────────────────
// Converts mixed tabs/spaces to 4-space indentation consistently,
// and trims trailing whitespace.

QString CodeFormatter::normaliseIndent(const QString& code, const QString& lang)
{
    // Languages that prefer 2-space indent
    const bool twoSpace = (lang == "yaml" || lang == "html" || lang == "css" ||
                           lang == "javascript" || lang == "typescript" ||
                           lang == "js" || lang == "ts" || lang == "json" ||
                           lang == "ruby" || lang == "graphql");
    const int  tabWidth = twoSpace ? 2 : 4;
    const QString indentStr(tabWidth, ' ');

    QStringList lines = code.split('\n');
    QStringList out;
    out.reserve(lines.size());

    for (QString line : lines) {
        // Measure existing indent (tabs count as tabWidth spaces)
        int spaces = 0;
        int i = 0;
        while (i < line.length()) {
            if (line[i] == '\t') {
                // Round up to next tab stop
                spaces = ((spaces / tabWidth) + 1) * tabWidth;
                ++i;
            } else if (line[i] == ' ') {
                ++spaces;
                ++i;
            } else {
                break;
            }
        }
        QString content = line.mid(i).trimmed(); // remove leading + trailing ws
        if (content.isEmpty()) {
            out << QString(); // preserve blank lines
            continue;
        }
        int level = spaces / tabWidth;
        out << indentStr.repeated(level) + content;
    }

    // Remove trailing blank lines
    while (!out.isEmpty() && out.last().trimmed().isEmpty())
        out.removeLast();

    return out.join('\n');
}
