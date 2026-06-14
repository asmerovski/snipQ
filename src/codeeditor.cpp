#include "codeeditor.h"
#include "appsettings.h"
#include <QPainter>
#include <QTextBlock>
#include <QScrollBar>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QFontMetrics>
#include <QRegularExpression>
#include <QMap>

// ── Language comment prefixes ─────────────────────────────────────────────────
static const QMap<QString,QString> COMMENT_PREFIX = {
    {"c","// "},{"cpp","// "},{"csharp","// "},{"java","// "},
    {"javascript","// "},{"typescript","// "},{"go","// "},{"rust","// "},
    {"kotlin","// "},{"swift","// "},{"scala","// "},{"dart","// "},
    {"objectivec","// "},{"php","// "},{"css","/* "},
    {"python","# "},{"ruby","# "},{"bash","# "},{"shell","# "},
    {"powershell","# "},{"r","# "},{"yaml","# "},{"toml","# "},
    {"dockerfile","# "},{"makefile","# "},{"nginx","# "},{"lua","-- "},
    {"sql","-- "},{"haskell","-- "},
    {"html","<!-- "},{"xml","<!-- "},
    {"latex","% "},{"markdown","<!-- "},
};

// ─── CodeEditor ───────────────────────────────────────────────────────────────

CodeEditor::CodeEditor(QWidget* parent)
    : QPlainTextEdit(parent)
{
    m_gutter = new LineNumberGutter(this);

    connect(this, &QPlainTextEdit::blockCountChanged,
            this, &CodeEditor::updateGutterWidth);
    connect(this, &QPlainTextEdit::updateRequest,
            this, &CodeEditor::updateGutterArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged,
            this, &CodeEditor::onCursorPositionChanged);
    connect(&AppSettings::instance(), &AppSettings::fontChanged,
            this, [this](const QFont& f) {
                setFont(f);
                QFontMetrics fm(f);
                setTabStopDistance(fm.horizontalAdvance(' ') * m_tabWidth);
                updateGutterWidth(blockCount());
            });

    updateGutterWidth(0);
    highlightCurrentLine();
}

void CodeEditor::setLanguage(const QString& lang)
{
    m_language = lang.toLower();
}

void CodeEditor::setTabWidth(int spaces)
{
    m_tabWidth = qBound(1, spaces, 8);
    QFontMetrics fm(font());
    setTabStopDistance(fm.horizontalAdvance(' ') * m_tabWidth);
}

// ─── Gutter ──────────────────────────────────────────────────────────────────

int CodeEditor::gutterWidth() const
{
    int digits = 1;
    int max    = qMax(1, blockCount());
    while (max >= 10) { max /= 10; ++digits; }
    digits = qMax(digits, 3);  // minimum 3 digits wide
    QFontMetrics fm(font());
    return fm.horizontalAdvance(QLatin1Char('9')) * digits + 20;
}

void CodeEditor::updateGutterWidth(int /*blockCount*/)
{
    setViewportMargins(gutterWidth(), 0, 0, 0);
}

void CodeEditor::updateGutterArea(const QRect& rect, int dy)
{
    if (dy)
        m_gutter->scroll(0, dy);
    else
        m_gutter->update(0, rect.y(), m_gutter->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateGutterWidth(0);
}

void CodeEditor::resizeEvent(QResizeEvent* event)
{
    QPlainTextEdit::resizeEvent(event);
    QRect cr = contentsRect();
    m_gutter->setGeometry(QRect(cr.left(), cr.top(), gutterWidth(), cr.height()));
}

void CodeEditor::paintGutter(QPaintEvent* event)
{
    QPainter painter(m_gutter);

    // Background
    painter.fillRect(event->rect(), QColor("#161b22"));

    // Right border
    painter.setPen(QColor("#21262d"));
    painter.drawLine(m_gutter->width() - 1, event->rect().top(),
                     m_gutter->width() - 1, event->rect().bottom());

    QTextBlock block     = firstVisibleBlock();
    int        blockNum  = block.blockNumber();
    int        top       = qRound(blockBoundingGeometry(block)
                                  .translated(contentOffset()).top());
    int        bottom    = top + qRound(blockBoundingRect(block).height());
    int        curLine   = textCursor().blockNumber();
    QFontMetrics fm(font());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            bool isCurrent = (blockNum == curLine);
            painter.setPen(isCurrent ? QColor("#c9d1d9") : QColor("#484f58"));

            QFont f = font();
            f.setBold(isCurrent);
            painter.setFont(f);

            painter.drawText(
                0, top,
                m_gutter->width() - 10, fm.height(),
                Qt::AlignRight,
                QString::number(blockNum + 1));
        }
        block  = block.next();
        top    = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNum;
    }
}

// ─── Current line highlight ───────────────────────────────────────────────────

void CodeEditor::onCursorPositionChanged()
{
    highlightCurrentLine();
}

void CodeEditor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extras;

    if (!isReadOnly()) {
        QTextEdit::ExtraSelection sel;
        // Subtle tint — just barely visible against the base background
        sel.format.setBackground(QColor("#161b22"));
        sel.format.setProperty(QTextFormat::FullWidthSelection, true);
        sel.cursor = textCursor();
        sel.cursor.clearSelection();
        extras << sel;
    }
    setExtraSelections(extras);
}

// ─── Key handling ─────────────────────────────────────────────────────────────

void CodeEditor::keyPressEvent(QKeyEvent* event)
{
    // Ctrl+/ — toggle line comment
    if (event->modifiers() == Qt::ControlModifier &&
        event->key() == Qt::Key_Slash)
    {
        QString prefix = commentPrefixForLang();
        if (!prefix.isEmpty()) {
            QTextCursor tc = textCursor();
            tc.beginEditBlock();

            int startBlock = tc.blockNumber();
            int endBlock   = startBlock;
            if (tc.hasSelection()) {
                QTextCursor tmp = tc;
                tmp.setPosition(tc.selectionStart());
                startBlock = tmp.blockNumber();
                tmp.setPosition(tc.selectionEnd());
                endBlock = tmp.blockNumber();
                // Don't include last block if cursor is at its start
                if (tc.selectionEnd() == tc.document()
                        ->findBlockByNumber(endBlock).position())
                    --endBlock;
            }

            // Check if all selected lines are already commented
            bool allCommented = true;
            for (int i = startBlock; i <= endBlock; ++i) {
                QTextBlock blk = tc.document()->findBlockByNumber(i);
                if (!blk.text().trimmed().startsWith(prefix.trimmed())) {
                    allCommented = false;
                    break;
                }
            }

            for (int i = startBlock; i <= endBlock; ++i) {
                QTextBlock blk = tc.document()->findBlockByNumber(i);
                QTextCursor lc(blk);

                if (allCommented) {
                    // Remove comment prefix
                    QString text = blk.text();
                    int     pos  = text.indexOf(prefix.trimmed());
                    if (pos >= 0) {
                        lc.setPosition(blk.position() + pos);
                        lc.movePosition(QTextCursor::Right,
                                        QTextCursor::KeepAnchor,
                                        prefix.length());
                        lc.removeSelectedText();
                    }
                } else {
                    // Add comment prefix at start of indented content
                    QString text    = blk.text();
                    int     indent  = 0;
                    while (indent < text.length() && text[indent].isSpace())
                        ++indent;
                    lc.setPosition(blk.position() + indent);
                    lc.insertText(prefix);
                }
            }
            tc.endEditBlock();
            return;
        }
    }

    // Tab key — insert spaces instead of a hard tab
    if (event->key() == Qt::Key_Tab && !event->modifiers()) {
        QTextCursor tc = textCursor();
        if (tc.hasSelection()) {
            // Indent selected lines
            int startBlock = tc.document()
                ->findBlock(tc.selectionStart()).blockNumber();
            int endBlock   = tc.document()
                ->findBlock(tc.selectionEnd()).blockNumber();
            // Don't indent last block if cursor is at its very start
            QTextBlock last = tc.document()->findBlockByNumber(endBlock);
            if (tc.selectionEnd() == last.position()) --endBlock;

            tc.beginEditBlock();
            for (int i = startBlock; i <= endBlock; ++i) {
                QTextBlock blk = tc.document()->findBlockByNumber(i);
                QTextCursor lc(blk);
                lc.movePosition(QTextCursor::StartOfBlock);
                lc.insertText(QString(m_tabWidth, ' '));
            }
            tc.endEditBlock();
        } else {
            // Insert spaces to next tab stop
            int col     = tc.columnNumber();
            int spaces  = m_tabWidth - (col % m_tabWidth);
            tc.insertText(QString(spaces, ' '));
        }
        return;
    }

    // Shift+Tab — unindent
    if (event->key() == Qt::Key_Backtab) {
        QTextCursor tc = textCursor();
        int startBlock = tc.document()
            ->findBlock(tc.selectionStart()).blockNumber();
        int endBlock   = tc.document()
            ->findBlock(tc.selectionEnd()).blockNumber();

        tc.beginEditBlock();
        for (int i = startBlock; i <= endBlock; ++i) {
            QTextBlock blk  = tc.document()->findBlockByNumber(i);
            QString    text = blk.text();
            int        rem  = 0;
            while (rem < m_tabWidth && rem < text.length() && text[rem] == ' ')
                ++rem;
            if (rem > 0) {
                QTextCursor lc(blk);
                lc.movePosition(QTextCursor::StartOfBlock);
                lc.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, rem);
                lc.removeSelectedText();
            }
        }
        tc.endEditBlock();
        return;
    }

    // Enter / Return — auto-indent to match current line
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        QTextCursor tc   = textCursor();
        QTextBlock  blk  = tc.block();
        QString     text = blk.text();

        // Measure leading whitespace
        int indent = 0;
        while (indent < text.length() && text[indent].isSpace())
            ++indent;

        // Extra indent if current line ends with an opening brace/bracket/colon
        QString trimmed = text.trimmed();
        bool    openBrace = !trimmed.isEmpty() &&
                            (trimmed.back() == '{' || trimmed.back() == '(' ||
                             trimmed.back() == '[' || trimmed.back() == ':');
        if (openBrace) indent += m_tabWidth;

        QPlainTextEdit::keyPressEvent(event);  // insert the newline
        textCursor().insertText(QString(indent, ' '));
        return;
    }

    // Closing brace — dedent if the line is only whitespace so far
    if ((event->key() == Qt::Key_BraceRight ||
         event->key() == Qt::Key_BracketRight ||
         event->key() == Qt::Key_ParenRight) &&
        !event->modifiers())
    {
        QTextCursor tc   = textCursor();
        QTextBlock  blk  = tc.block();
        QString     text = blk.text();
        bool allSpace = !text.isEmpty() &&
                        std::all_of(text.begin(), text.end(),
                                    [](QChar c){ return c.isSpace(); });
        if (allSpace && text.length() >= m_tabWidth) {
            tc.beginEditBlock();
            tc.movePosition(QTextCursor::StartOfBlock);
            tc.movePosition(QTextCursor::Right,
                            QTextCursor::KeepAnchor, m_tabWidth);
            tc.removeSelectedText();
            tc.endEditBlock();
        }
    }

    QPlainTextEdit::keyPressEvent(event);
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

QString CodeEditor::commentPrefixForLang() const
{
    return COMMENT_PREFIX.value(m_language, QString());
}
