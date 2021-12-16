#ifndef WORKER_H
#define WORKER_H

#include <QImage>
#include <QObject>

#include "MathematicaL.h"
#include "Camera.h"


class Worker : public QObject
{
    Q_OBJECT

public:
    Worker(QObject* parent = nullptr, bool test = false);
    void initialize(bool full = false);
    void deinitialize();
    void toggle_camera();

    ~Worker();

    void start();
    void stop();
    void reset(bool full=false);
    void received_image(std::shared_ptr<Image>);

signals:
    void display_image(QImage img);

private:
    bool m_stop        = false;
    bool m_poll_wstp   = true;
    bool m_running     = false;
    bool m_test        = false;
    bool m_camera_flag = false;
    int m_reset        = 0;
    Camera *m_camera;
    MathematicaL m_link;

    std::shared_ptr<Image> get_camera_image();
    int yield_callback(State, int);


    int m_imageWidth = 0;
    int m_imageHeight = 0;
    int m_cropx = 0;
    int m_cropy = 0;
    int m_cropw = -1;
    int m_croph = -1;

};

#endif
