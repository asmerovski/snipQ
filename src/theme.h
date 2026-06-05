#pragma once
#include <QString>

// snipQ dark theme — inspired by MassCode 3's aesthetics
namespace Theme {

inline const QString StyleSheet = R"(
/* ── Global ── */
* {
    font-family: "JetBrains Mono", "Cascadia Code", "Fira Code", "Consolas", monospace;
    font-size: 13px;
    color: #c9d1d9;
    selection-background-color: #2d4a6e;
    selection-color: #e6edf3;
}

QMainWindow, QWidget {
    background-color: #0d1117;
}

/* ── Sidebar ── */
#Sidebar {
    background-color: #161b22;
    border-right: 1px solid #21262d;
    min-width: 220px;
    max-width: 300px;
}

QTreeWidget {
    background-color: transparent;
    border: none;
    outline: none;
    padding: 4px 0;
}

QTreeWidget::item {
    padding: 5px 8px;
    border-radius: 6px;
    margin: 1px 4px;
    color: #8b949e;
}

QTreeWidget::item:hover {
    background-color: #21262d;
    color: #c9d1d9;
}

QTreeWidget::item:selected {
    background-color: #1f3352;
    color: #58a6ff;
}

QTreeWidget::branch {
    background-color: transparent;
}

QTreeWidget::branch:has-children:!has-siblings:closed,
QTreeWidget::branch:closed:has-children:has-siblings {
    image: url(:/icons/chevron-right.svg);
}

QTreeWidget::branch:open:has-children:!has-siblings,
QTreeWidget::branch:open:has-children:has-siblings {
    image: url(:/icons/chevron-down.svg);
}

/* ── Snippet List Panel ── */
#SnippetList {
    background-color: #0d1117;
    border-right: 1px solid #21262d;
    min-width: 240px;
    max-width: 320px;
}

#SnippetListSearch {
    background-color: #161b22;
    border: 1px solid #30363d;
    border-radius: 6px;
    padding: 6px 10px;
    color: #c9d1d9;
    margin: 8px;
}

#SnippetListSearch:focus {
    border-color: #388bfd;
}

#SnippetListSearch::placeholder {
    color: #484f58;
}

QListWidget {
    background-color: transparent;
    border: none;
    outline: none;
}

QListWidget::item {
    padding: 0;
    margin: 0;
    border-bottom: 1px solid #21262d;
}

QListWidget::item:selected {
    background-color: #161b22;
}

QListWidget::item:hover:!selected {
    background-color: #161b22;
}

/* ── Editor Panel ── */
#EditorPanel {
    background-color: #0d1117;
}

#SnippetTitle {
    background-color: transparent;
    border: none;
    border-bottom: 1px solid #21262d;
    padding: 12px 16px;
    font-size: 16px;
    font-weight: 600;
    color: #e6edf3;
}

#SnippetTitle:focus {
    border-bottom-color: #388bfd;
    background-color: transparent;
}

QPlainTextEdit {
    background-color: #0d1117;
    border: none;
    padding: 16px;
    color: #c9d1d9;
    font-family: "JetBrains Mono", "Cascadia Code", "Fira Code", monospace;
    font-size: 13px;
    line-height: 1.6;
    selection-background-color: #2d4a6e;
}

QPlainTextEdit:focus {
    outline: none;
}

/* ── Tab Bar ── */
QTabBar {
    background-color: #161b22;
    border-bottom: 1px solid #21262d;
}

QTabBar::tab {
    background-color: transparent;
    color: #8b949e;
    padding: 8px 16px;
    border-bottom: 2px solid transparent;
    margin-right: 2px;
}

QTabBar::tab:selected {
    color: #58a6ff;
    border-bottom-color: #388bfd;
}

QTabBar::tab:hover:!selected {
    color: #c9d1d9;
    background-color: #21262d;
}

QTabWidget::pane {
    border: none;
    background-color: #0d1117;
}

/* ── Toolbar ── */
QToolBar {
    background-color: #161b22;
    border-bottom: 1px solid #21262d;
    spacing: 4px;
    padding: 4px 8px;
}

QToolButton {
    background-color: transparent;
    border: 1px solid transparent;
    border-radius: 6px;
    padding: 5px 8px;
    color: #8b949e;
}

QToolButton:hover {
    background-color: #21262d;
    border-color: #30363d;
    color: #c9d1d9;
}

QToolButton:pressed {
    background-color: #2d4a6e;
}

/* ── Splitter ── */
QSplitter::handle {
    background-color: #21262d;
    width: 1px;
    height: 1px;
}

QSplitter::handle:hover {
    background-color: #388bfd;
}

/* ── Scrollbar ── */
QScrollBar:vertical {
    background-color: transparent;
    width: 8px;
    margin: 0;
}

QScrollBar::handle:vertical {
    background-color: #30363d;
    border-radius: 4px;
    min-height: 24px;
}

QScrollBar::handle:vertical:hover {
    background-color: #484f58;
}

QScrollBar::add-line:vertical,
QScrollBar::sub-line:vertical,
QScrollBar::add-page:vertical,
QScrollBar::sub-page:vertical {
    background: none;
    height: 0;
}

QScrollBar:horizontal {
    background-color: transparent;
    height: 8px;
}

QScrollBar::handle:horizontal {
    background-color: #30363d;
    border-radius: 4px;
    min-width: 24px;
}

QScrollBar::handle:horizontal:hover {
    background-color: #484f58;
}

QScrollBar::add-line:horizontal,
QScrollBar::sub-line:horizontal {
    background: none;
    width: 0;
}

/* ── Menus ── */
QMenu {
    background-color: #161b22;
    border: 1px solid #30363d;
    border-radius: 8px;
    padding: 4px;
}

QMenu::item {
    padding: 6px 20px 6px 12px;
    border-radius: 4px;
    color: #c9d1d9;
}

QMenu::item:selected {
    background-color: #1f3352;
    color: #58a6ff;
}

QMenu::separator {
    background-color: #21262d;
    height: 1px;
    margin: 4px 8px;
}

QMenuBar {
    background-color: #161b22;
    border-bottom: 1px solid #21262d;
}

QMenuBar::item {
    padding: 6px 12px;
    color: #8b949e;
}

QMenuBar::item:selected {
    background-color: #21262d;
    color: #c9d1d9;
}

/* ── Dialogs ── */
QDialog {
    background-color: #161b22;
    border: 1px solid #30363d;
    border-radius: 10px;
}

QLabel {
    color: #8b949e;
    background: transparent;
}

QLineEdit, QComboBox, QTextEdit {
    background-color: #0d1117;
    border: 1px solid #30363d;
    border-radius: 6px;
    padding: 6px 10px;
    color: #c9d1d9;
}

QLineEdit:focus, QComboBox:focus, QTextEdit:focus {
    border-color: #388bfd;
}

QPushButton {
    background-color: #21262d;
    border: 1px solid #30363d;
    border-radius: 6px;
    padding: 6px 16px;
    color: #c9d1d9;
    font-weight: 500;
}

QPushButton:hover {
    background-color: #30363d;
    border-color: #8b949e;
}

QPushButton:pressed {
    background-color: #2d4a6e;
}

QPushButton#primaryButton {
    background-color: #1f6feb;
    border-color: #388bfd;
    color: #ffffff;
}

QPushButton#primaryButton:hover {
    background-color: #388bfd;
}

QPushButton#dangerButton {
    background-color: #3d1616;
    border-color: #f85149;
    color: #f85149;
}

QPushButton#dangerButton:hover {
    background-color: #5a1e1e;
}

/* ── Status bar ── */
QStatusBar {
    background-color: #161b22;
    border-top: 1px solid #21262d;
    color: #484f58;
    font-size: 11px;
    padding: 2px 8px;
}

/* ── Checkboxes ── */
QCheckBox {
    color: #c9d1d9;
    spacing: 8px;
}

QCheckBox::indicator {
    width: 16px;
    height: 16px;
    border: 1px solid #30363d;
    border-radius: 4px;
    background-color: #0d1117;
}

QCheckBox::indicator:checked {
    background-color: #1f6feb;
    border-color: #388bfd;
    image: url(:/icons/check.svg);
}

/* ── Tag chips ── */
#TagsBar {
    background-color: #0d1117;
    border-top: 1px solid #21262d;
    padding: 6px 12px;
}

/* ── Language selector ── */
QComboBox {
    background-color: #161b22;
    border: 1px solid #30363d;
    border-radius: 6px;
    padding: 4px 8px;
    min-width: 120px;
}

QComboBox::drop-down {
    border: none;
    width: 20px;
}

QComboBox QAbstractItemView {
    background-color: #161b22;
    border: 1px solid #30363d;
    selection-background-color: #1f3352;
}

/* ── GroupBox (settings sections) ── */
QGroupBox {
    border: 1px solid #21262d;
    border-radius: 8px;
    margin-top: 16px;
    padding: 12px;
    font-weight: 600;
    color: #8b949e;
}

QGroupBox::title {
    subcontrol-origin: margin;
    left: 12px;
    padding: 0 4px;
    color: #58a6ff;
}

/* ── Tooltips ── */
QToolTip {
    background-color: #161b22;
    border: 1px solid #30363d;
    border-radius: 6px;
    color: #c9d1d9;
    padding: 4px 8px;
}
)";

namespace Colors {
    inline const QString BgBase      = "#0d1117";
    inline const QString BgSurface   = "#161b22";
    inline const QString BgElevated  = "#21262d";
    inline const QString Border      = "#30363d";
    inline const QString TextPrimary = "#e6edf3";
    inline const QString TextMuted   = "#8b949e";
    inline const QString Accent      = "#388bfd";
    inline const QString AccentDark  = "#1f6feb";
    inline const QString Danger      = "#f85149";
    inline const QString Success     = "#3fb950";
    inline const QString Warning     = "#d29922";
    inline const QString Star        = "#e3b341";
}

} // namespace Theme
