#include <QApplication>
#include <QStyleFactory>
#include "mainwindow.h"
#include "theme.h"

int main(int argc, char* argv[]) {
    // ── Must be set BEFORE QApplication is constructed ──
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);

    app.setApplicationName("snipQ");
    app.setOrganizationName("snipQ");
    app.setApplicationVersion("1.0.0");
    app.setWindowIcon(QIcon(":/icons/app.png"));

    // Fusion base style for consistent cross-platform rendering
    app.setStyle(QStyleFactory::create("Fusion"));

    // Apply dark theme
    app.setStyleSheet(Theme::StyleSheet);

    MainWindow win;
    win.show();

    return app.exec();
}
