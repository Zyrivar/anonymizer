// main_gui.cpp — точка входа для Qt5 GUI анонимизатора.
#include "mainwindow.h"
#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("C++ Anonymizer");
    app.setOrganizationName("anon");

    MainWindow w;
    w.show();

    return app.exec();
}
