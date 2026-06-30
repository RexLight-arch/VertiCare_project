#include "mainwindow.h"
#include <stdio.h>
#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/icons/1.png"));

    MainWindow window;
    window.resize(1220, 760);
    window.show();

    return app.exec();
}
