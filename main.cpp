#include <QApplication>
#include <QTimer>
#include <QDir>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow window;
    window.show();

    for (int i = 1; i < argc; ++i)
    {
        if (QString(argv[i]) == "--screenshot")
        {
            QTimer::singleShot(300, [&]() {
                QDir().mkpath("assets");
                QPixmap pixmap = window.grab();
                pixmap.save("assets/ui_screenshot.png");
                app.quit();
            });
            break;
        }
    }

    return app.exec();
}