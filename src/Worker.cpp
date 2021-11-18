#include <cmath>
#include <cstring>
#include <exception>
#include <thread>
#include <algorithm>

#include <QDebug>
#include <QObject>

#include "Worker.h"
#include "Camera.h"
#include "MathematicaL.h"

Worker::Worker(QObject* parent, bool test)
    : QObject(parent),
      m_link (MathematicaL("ueye")),
      m_test (test)
{
    m_stop      = false;
    m_poll_wstp = true;
    m_camera    = nullptr;

    //m_link.register_yield_callback(std::bind(&Worker::yield_callback, *this, 
    //            std::placeholders::_2, std::placeholders::_3));
    //m_link.register_hook_get(std::bind(&Worker::get_camera_image,*this));
    //std::function<int(State,int)> fn = [this](State s, int c) {
    m_link.register_yield_callback([this](State s, int c) {
        return this->yield_callback(s, c);
    });

    //std::function<std::shared_ptr<Image>()> fn2 = ;
    m_link.register_hook_get([this]() {
        return this->get_camera_image();
    });

    m_link.register_hook_send([this](std::shared_ptr<Image> img){
                this->received_image(img);
            });

    m_link.register_hook_set_prop([this](const std::string &prop, double val){
            try {
                if (m_camera) {
                    m_camera->set_double_property(prop, val);
                }
            } catch(const char *e) {
                fprintf(stderr, "Worker: failed to set property %s: %s\n", prop.c_str(), e);
            } catch(const std::exception &e) {
                fprintf(stderr, "Worker: failed to set property %s: %s\n", prop.c_str(), e.what());
            }

            });

    m_link.register_hook_imgcrop([this](int x, int y, int w, int h){
            m_cropx = x;
            m_cropy = y;
            m_cropw = w;
            m_croph = h;
        });

}


void Worker::received_image(std::shared_ptr<Image> img)
{
    if (!img) return;

    printf("Worker: received %lu x %lu image\n", img->w, img->h);
    int *buffer = img->release();
    QImage qimg{(uchar*)(buffer), (int)img->w, (int)img->h, QImage::Format_RGB32,
            [] (void *ptr) {
                delete[] (int*)ptr;
            },
            buffer
        };
    emit display_image(qimg);
}

int Worker::yield_callback(State state, int count)
{
    if (!m_stop && m_poll_wstp && !m_reset)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return 0;
    }

    return 1;
}

std::shared_ptr<Image> Worker::get_camera_image()
{
    try
    {
        printf("Woker: getting camera image\n");
        ptr_image img {m_camera->get_image()};
        if (img)
        {

            int x = std::clamp(m_cropx, 0, static_cast<int>(img->w));
            int y = std::clamp(m_cropy, 0, static_cast<int>(img->h));

            int w = (m_cropw < 0 ? static_cast<int>(img->w) : m_cropw);
            int h = (m_croph < 0 ? static_cast<int>(img->w) : m_croph);
            w = std::clamp(w, 0, static_cast<int>(img->w) - x);
            h = std::clamp(h, 0, static_cast<int>(img->h) - y);

            ptr_image c_img = std::make_shared<Image>(new int[w*h], w, h);

            printf("Worker: cropped at %i, %i with %ix%i\n", x, y, w, h);
            const int *ptr = img->data + x + y*img->w;
            int *dst = c_img->data;
            for (size_t j = y; j < h; j++)
            {
                memcpy(dst, ptr, w*sizeof(int));
                ptr += img->w;
                dst += w;
            }

            return c_img;
        }

        return img;
    }
    catch (const char *e)
    {
        fprintf(stderr, "Worker: failed to get image from camera - %s\n",e);
        return nullptr;
    }
    catch (std::exception &e)
    {
        fprintf(stderr, "Worker: failed to get image from camera - %s\n",e.what());
        return nullptr;
    }
}


Worker::~Worker()
{
    deinitialize();
    delete m_camera;
}

void Worker::initialize(bool full)
{
    printf("\nWorker: initializing ...\n");
    try {
        if (!m_camera)
        {
            if (m_test)
                m_camera = new TestCamera(760, 480);
            else
                m_camera = new IDSCamera();
        }
        else
        {
            try {
                m_camera->deinitialize();
            } catch (const char *e){
            } catch (std::exception &e){}
        }

        printf("initialzing camera\n");
        m_camera->initialize();
    } catch (const char *e) {
        fprintf(stderr, "Worker: failed to initialize camera - %s\n", e);
    } catch (std::exception &e) {
        fprintf(stderr, "Worker: failed to initialize camera - %s\n", e.what());
    }

    m_link.initialize_connection(full);
}

void Worker::deinitialize()
{
    try {
        if (!m_camera)
        {
            m_camera->deinitialize();
        }
    } catch (const char *e) {
        fprintf(stderr, "Worker: failed to deinitialize camera - %s\n", e);
    } catch (std::exception &e) {
        fprintf(stderr, "Worker: failed to deinitialize camera - %s\n", e.what());
    }
     m_link.deinitialize_close();
}

void Worker::reset(bool full)
{
    m_reset = full ? 2 : 1;
}


void Worker::start()
{
    int ret;
    int tries = 0;
    m_running = true;

    initialize(true);

    while (!m_stop)
    {
        if (m_reset)
        {
            initialize(m_reset == 2);
            m_reset = tries = 0;
        } 

        m_link.set_verbosity(tries < 2);
        LinkState state = m_link.check_error();
        if (state == DEFERRED) {
            printf("Worker: waiting for connection ... \n");
            try {
                m_link.activate_link();
            } catch(std::exception &e) {
                fprintf(stderr, "Worker: %s\n",e.what());
            }
            continue;
        }

        ret = m_link.clear_error(state);
        m_link.set_verbosity(1);

        if (ret)
        {
            if (tries < 2)
            {
                fprintf(stderr, "main: unable to clear error\n");
                ++tries;
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

        }
        else
        {
            printf("\n");
            ret = m_link.process_packet();
            if (ret == -1) {
                break;
            } else if (!ret) {
                tries = 0;
            }
        }
    }
    deinitialize();
    printf("Worker: main loop ending\n");
}

void Worker::stop()
{
    m_poll_wstp = false;
    m_stop      = true;
}
