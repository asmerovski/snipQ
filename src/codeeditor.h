#pragma once
#include <QPlainTextEdit>
#include <QWidget>
#include <QColor>

// QPlainTextEdit subclass that adds:
//  • Line number gutter (left margin)
//  • Tab key inserts spaces (respects AppSettings indent width)
//  • Auto-indent: Enter preserves leading whitespace of current line
//  • Highlight of current line (subtle tint)
//  • Ctrl+/ toggles line comment based on language

class CodeEditor : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit CodeEditor(QWidget* parent = nullptr);

    void setLanguage(const QString& lang);
    void setTabWidth(int spaces);   // default 4

    // Called by the gutter widget
    int  gutterWidth() const;
    void paintGutter(QPaintEvent* event);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onCursorPositionChanged();
    void updateGutterWidth(int blockCount);
    void updateGutterArea(const QRect& rect, int dy);

private:
    void  highlightCurrentLine();
    int   indentLevelAt(const QTextBlock& block) const;
    QString commentPrefixForLang() const;

    QWidget* m_gutter;
    int      m_tabWidth  = 4;
    QString  m_language  = "plaintext";
};

// ── Gutter widget ─────────────────────────────────────────────────────────────
class LineNumberGutter : public QWidget {
public:
    explicit LineNumberGutter(CodeEditor* editor) : QWidget(editor), m_editor(editor) {}
    QSize sizeHint() const override { return QSize(m_editor->gutterWidth(), 0); }

protected:
    void paintEvent(QPaintEvent* event) override { m_editor->paintGutter(event); }

private:
    CodeEditor* m_editor;
};
