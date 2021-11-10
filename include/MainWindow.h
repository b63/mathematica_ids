#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "Worker.h"
#include "Display.h"

#include <QLabel>
#include <QMainWindow>
#include <QMessageBox>
#include <QThread>
#include <QVBoxLayout>
#include <QWidget>

#include <cstdint>


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

    void keyPressEvent(QKeyEvent*);

private:
    ImageDisplay* m_display;
    QLabel* m_labelInfo;
    QVBoxLayout* m_layout;

    Worker* m_worker;
    QThread m_thread;
    bool m_title;

    void destroy_all();
    void destroy_quit();
    void toggle_title_bar(bool state);

};

#endif
