#pragma once
#include <QObject>
#include <QFont>
#include <QSettings>
#include <QFontInfo>

// Singleton that holds all user-configurable preferences.
// Other widgets connect to fontChanged() to update live.
class AppSettings : public QObject {
    Q_OBJECT

public:
    static AppSettings& instance() {
        static AppSettings s;
        return s;
    }

    // ── Editor font ───────────────────────────────────────────
    QFont editorFont() const { return m_editorFont; }

    void setEditorFont(const QFont& f) {
        if (m_editorFont == f) return;
        m_editorFont = f;
        save();
        emit fontChanged(m_editorFont);
    }

    // ── Persistence ──────────────────────────────────────────
    void load() {
        QSettings s("snipQ", "snipQ");
        QString family = s.value("editor/fontFamily", "").toString();
        int     size   = s.value("editor/fontSize",   13).toInt();

        m_editorFont = makeMono(family, size);
    }

    void save() const {
        QSettings s("snipQ", "snipQ");
        s.setValue("editor/fontFamily", m_editorFont.family());
        s.setValue("editor/fontSize",   m_editorFont.pointSize());
    }

    // Build a verified monospace font (falls back if family isn't fixed-pitch)
    static QFont makeMono(const QString& family, int pointSize = 13) {
        QFont f;
        f.setStyleHint(QFont::Monospace);
        f.setFixedPitch(true);
        f.setPointSize(qBound(8, pointSize, 32));

        if (!family.isEmpty()) {
            f.setFamily(family);
            if (QFontInfo(f).fixedPitch()) return f;
        }
        // Fallback chain
        static const QStringList chain = {
            "JetBrains Mono","Cascadia Code","Fira Code","Hack",
            "Source Code Pro","Consolas","Courier New","monospace"
        };
        for (const QString& name : chain) {
            f.setFamily(name);
            if (QFontInfo(f).fixedPitch()) return f;
        }
        return f;
    }

signals:
    void fontChanged(const QFont& font);

private:
    AppSettings() { load(); }
    AppSettings(const AppSettings&)            = delete;
    AppSettings& operator=(const AppSettings&) = delete;

    QFont m_editorFont;
};
