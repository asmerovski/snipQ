#include <QApplication>
#include <QStyleFactory>
#include "mainwindow.h"
#include "theme.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    app.setApplicationName("snipQ");
    app.setOrganizationName("snipQ");
    app.setApplicationVersion("1.0.0");

    // Use Fusion base style for cross-platform consistency
    app.setStyle(QStyleFactory::create("Fusion"));

    // Apply our dark theme
    app.setStyleSheet(Theme::StyleSheet);

    // High-DPI
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    app.setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif

    MainWindow win;
    win.show();

    return app.exec();
}
