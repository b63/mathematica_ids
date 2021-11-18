#include "MainWindow.h"

#include <QApplication>


int main(int argc, char* argv[])
{
    bool test = false;
    if (argc > 1)
    {
        test = !strcmp(argv[1], "--test");
    }

    QApplication a(argc, argv);
    MainWindow w (nullptr, test);
    w.show();

    return a.exec();
}

