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
    // Use Fusion base style for cross-platform consistency
    // app.setStyle(QStyleFactory::create("Fusion"));

    // Apply our dark theme
    // app.setStyleSheet(Theme::StyleSheet);


    MainWindow win;
    win.show();

    return app.exec();
}
