#ifndef CAMERA_H
#define CAMERA_H

#include <string>
#include <vector>
#include <memory>

#include <peak_ipl/peak_ipl.hpp>
#include <peak/peak.hpp>

#include "util.h"

using ptr_image = std::shared_ptr<Image>;

struct Camera {
    public:
        virtual ~Camera();
        virtual void initialize() = 0;
        virtual void deinitialize() = 0;
        virtual void set_double_property(const std::string &name, double value) = 0;
        virtual ptr_image get_image() = 0;
};

class TestCamera : public Camera 
{
    public:
        TestCamera(size_t width, size_t height);
        void initialize() override;
        void deinitialize() override;
        void set_double_property(const std::string &name, double value) override;
        ptr_image get_image() override;

    private:
        size_t m_width;
        size_t m_height;
        double m_exposure;
};


class IDSCamera : public Camera
{
    public:
        IDSCamera();
        ~IDSCamera();
        void initialize() override;
        void deinitialize() override;
        void set_double_property(const std::string &name, double value) override;
        ptr_image get_image() override;

    private:
        double m_exposure;
        std::shared_ptr<peak::core::Device> m_device;
        std::shared_ptr<peak::core::DataStream> m_stream;
        std::shared_ptr<peak::core::NodeMap> m_nodemap;

        void open_device();
        void open_data_stream();
        void reset_params();

};

#endif
