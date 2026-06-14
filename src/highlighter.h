#pragma once
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QColor>
#include <QString>
#include <QVector>

namespace HlColor {
    static const QColor Keyword      { 0xff, 0x79, 0xc6 };
    static const QColor Type         { 0x8b, 0xe9, 0xfd };
    static const QColor Function     { 0x50, 0xfa, 0x7b };
    static const QColor String       { 0xf1, 0xfa, 0x8c };
    static const QColor Number       { 0xbd, 0x93, 0xf9 };
    static const QColor Comment      { 0x6a, 0x73, 0x7d };
    static const QColor Preprocessor { 0xff, 0xb8, 0x6c };
    static const QColor Operator     { 0xff, 0x79, 0xc6 };
    static const QColor Constant     { 0xbd, 0x93, 0xf9 };
    static const QColor Tag          { 0x58, 0xa6, 0xff };
    static const QColor Attribute    { 0x79, 0xc0, 0xff };
    static const QColor Escape       { 0xff, 0xb8, 0x6c };
}

class Highlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit Highlighter(QTextDocument* parent = nullptr);
    void setLanguage(const QString& lang);
    QString language() const { return m_lang; }

    // ── Public helpers (used by language setup free functions) ──
    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat    format;
        int                captureGroup = 0;
    };

    struct MultiLineComment {
        QRegularExpression start;
        QRegularExpression end;
        QTextCharFormat    format;
        int                stateId;
    };

    static QTextCharFormat hlFmt(const QColor& c,
                                  bool bold   = false,
                                  bool italic = false);

    void addHlRule(const QString& pattern,
                   const QTextCharFormat& fmt,
                   int captureGroup = 0);

    void addHlKeywords(const QStringList& words,
                       const QTextCharFormat& fmt);

    void addMultiLineComment(const MultiLineComment& mlc);

    QVector<Rule>&             rules()      { return m_rules; }
    QVector<MultiLineComment>& mlComments() { return m_mlComments; }

protected:
    void highlightBlock(const QString& text) override;

private:
    void clearRules();

    void setupPlaintext();
    void setupC();
    void setupCpp();
    void setupCsharp();
    void setupJava();
    void setupJavaScript();
    void setupTypeScript();
    void setupPython();
    void setupRust();
    void setupGo();
    void setupKotlin();
    void setupSwift();
    void setupScala();
    void setupDart();
    void setupRuby();
    void setupPhp();
    void setupBash();
    void setupShell();
    void setupPowerShell();
    void setupSql();
    void setupJson();
    void setupXml();
    void setupHtml();
    void setupCss();
    void setupYaml();
    void setupToml();
    void setupMarkdown();
    void setupDiff();
    void setupDockerfile();
    void setupMakefile();
    void setupNginx();
    void setupLatex();
    void setupLua();
    void setupR();
    void setupGraphql();
    void setupHttp();
    void setupObjectiveC();

    QString                   m_lang;
    QVector<Rule>             m_rules;
    QVector<MultiLineComment> m_mlComments;
};
