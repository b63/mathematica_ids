#include <QHBoxLayout>
#include <QLabel>
#include <QKeyEvent>
#include <QMainWindow>
#include <QMessageBox>
#include <QThread>
#include <QWidget>

#include <cstdint>

#include "MainWindow.h"
#include "Worker.h"
#include "Display.h"


MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    QWidget* widget = new QWidget(this);

    m_layout = new QVBoxLayout;
    widget->setLayout(m_layout);
    setCentralWidget(widget);

    m_layout->setContentsMargins(0,0,0,0);
    setWindowFlags(Qt::CustomizeWindowHint);

    // Create a display for the camera image
    m_display = new ImageDisplay(widget);
    m_layout->addWidget(m_display);

    // Create worker thread that waits for new images from the camera
    m_worker = new Worker();
    m_worker->moveToThread(&m_thread);

    // worker must be started, when the acquisition starts, and deleted, when the worker thread finishes
    connect(&m_thread, &QThread::started, m_worker, &Worker::start);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(&m_thread, &QThread::finished, this, &MainWindow::destroy_quit);

    connect(m_worker, &Worker::display_image, m_display, &ImageDisplay::onImageReceived);

    // Start thread execution
    m_thread.start();

    // Set minimum window size
    this->setMinimumSize(700, 500);
}


void MainWindow::keyPressEvent(QKeyEvent *event) 
{
    if (!event)
    {
        QMainWindow::keyPressEvent(event);
        return;
    }

    const int k {event->key()};

    if (k == Qt::Key_R)
    {
        m_worker->reset( (event->modifiers() & Qt::ShiftModifier) ? 1 : 0);
    }
    else if (k == Qt::Key_F)
    {
        m_display->set_display_mode(m_display->get_display_mode() == NO_FILL 
                ? FILL_FIT : NO_FILL);
    }
    else if (k == Qt::Key_Q)
    {
        destroy_all();
    }
    else
    {
        QMainWindow::keyPressEvent(event);
    }
}

void MainWindow::destroy_quit()
{

    destroy_all();
    exit(0);
}


MainWindow::~MainWindow()
{
    destroy_all();
    delete m_display;
}


void MainWindow::destroy_all()
{
    if (m_worker)
    {
        m_worker->stop();
        m_thread.quit();
        m_thread.wait();
    }
}
