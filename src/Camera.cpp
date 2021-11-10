#include <cstdio>
#include <random>
#include <time.h>

#include <peak_ipl/peak_ipl.hpp>
#include <peak/converters/peak_buffer_converter_ipl.hpp>

#include <peak/peak.hpp>

#include "Camera.h"

Camera::~Camera()
{
}

TestCamera::TestCamera(size_t width, size_t height)
    : Camera(), m_width (width), m_height(height)
{ }

void TestCamera::initialize()
{
    printf("TestCamera: initialize()\n");
}

void TestCamera::deinitialize()
{
    printf("TestCamera: deinitialize()\n");
}

void TestCamera::set_double_property(const std::string &name, double value)
{
    m_exposure = value;

    printf("TestCamera: exposure set to %.2f\n", m_exposure);
}

ptr_image TestCamera::get_image()
{
    static std::default_random_engine engine;
    static std::uniform_int_distribution<int> dist(0, 255);

    const size_t N = m_height*m_width;
    ptr_image img{std::make_shared<Image>(new int[N], m_width, m_height)};


    for (auto i = 0; i < N; ++i)
        (img->data)[i]=((dist(engine) << 16)
                | (dist(engine) << 8) 
                | (dist(engine)));

    return img;
}

IDSCamera::IDSCamera()
    : Camera()
{
    m_exposure = -1;
    peak::Library::Initialize();
}


IDSCamera::~IDSCamera()
{
    deinitialize();
    peak::Library::Close();
}
void IDSCamera::initialize()
{
    open_device();
    m_nodemap = m_device->RemoteDevice()->NodeMaps().at(0);
}

void IDSCamera::set_double_property(const std::string &name, double value)
{
    if(!m_nodemap) throw "NodeMap unavailable, device uninitialized";
    if (name == "exposure")
    {
        m_nodemap->FindNode<peak::core::nodes::FloatNode>("ExposureTime")->SetValue(value);
        m_exposure = value;
    }
    else if (name == "gain")
        m_nodemap->FindNode<peak::core::nodes::FloatNode>("Gain")->SetValue(value);
    else
        fprintf(stderr, "IDSCamera: unkown property %s\n", name.c_str());
}

ptr_image IDSCamera::get_image()
{
    open_data_stream();
    m_stream->StartAcquisition(peak::core::AcquisitionStartMode::Default, 1);
    m_nodemap->FindNode<peak::core::nodes::IntegerNode>("TLParamsLocked")->SetValue(1);
    m_nodemap->FindNode<peak::core::nodes::CommandNode>("AcquisitionStart")->Execute();

    ptr_image img;

    try {
        const auto buffer = m_stream->WaitForFinishedBuffer(m_timeout);

        const auto image = peak::BufferTo<peak::ipl::Image>(buffer);
        size_t w = image.Width();
        size_t h = image.Height();

        img = std::make_shared<Image>(new int[w*h], w, h);

        image.ConvertTo(
            peak::ipl::PixelFormatName::BGRa8, (uint8_t*)img->data, 
            static_cast<size_t>(img->w*img->h*sizeof(int)), 
            peak::ipl::ConversionMode::Fast);

        // Queue buffer so that it can be used again
        m_stream->QueueBuffer(buffer);
    }
    catch (const peak::core::TimeoutException& e)
    {
        fprintf(stderr, "IDSCamera: timeout during acquisition - %s\n", e.what());
        return nullptr;
    }
    catch (const std::exception& e)
    {
        fprintf(stderr, "IDSCamera: failed acquisition - %s\n", e.what());
        return nullptr;
    }

    m_nodemap->FindNode<peak::core::nodes::CommandNode>("AcquisitionStop")->Execute();
    m_nodemap->FindNode<peak::core::nodes::IntegerNode>("TLParamsLocked")->SetValue(0);
    m_stream->StopAcquisition(peak::core::AcquisitionStopMode::Default);

    return img;
}

void IDSCamera::deinitialize()
{
    if(m_stream)
    {
        try
        {
            m_stream->StopAcquisition(peak::core::AcquisitionStopMode::Default);
            m_stream->KillWait();
            m_stream->Flush(peak::core::DataStreamFlushMode::DiscardAll);

            for (const auto& buffer : m_stream->AnnouncedBuffers())
            {
                m_stream->RevokeBuffer(buffer);
            }
        } catch (const std::exception &e)
        {
            fprintf(stderr, "IDSCamera: failed to release buffers - %s\n", e.what());
        }
    }

    if (m_nodemap)
    {
        m_nodemap->FindNode<peak::core::nodes::IntegerNode>("TLParamsLocked")->SetValue(0);
    }

    m_stream  = nullptr;
    m_nodemap = nullptr;
    m_device  = nullptr;
}

void IDSCamera::open_data_stream()
{
    if (!m_device) throw "cannot open DataStream, device not initialized";
    if (!m_nodemap) throw "no NodeMap for device";

    // Open standard data stream
    if (m_stream)
    {
         // Flush queue and prepare all buffers for revoking
        //m_stream->StopAcquisition(peak::core::AcquisitionStopMode::Default);
        m_stream->Flush(peak::core::DataStreamFlushMode::DiscardAll);
        //m_stream->KillWait();

        // Clear all old buffers
        for (const auto& buffer : m_stream->AnnouncedBuffers())
        {
            m_stream->RevokeBuffer(buffer);
        }
    }
    else 
    {
        auto dataStreams = m_device->DataStreams();
            if (dataStreams.empty())
            {
                m_device.reset();
                throw "device has no DataStream";
            }

        m_stream = dataStreams.at(0)->OpenDataStream();
    }

    
    reset_params();

    // Get the payload size for correct buffer allocation
    auto payloadSize = m_nodemap->FindNode<peak::core::nodes::IntegerNode>("PayloadSize")
                            ->Value();

    // Get the minimum number of buffers that must be announced
    auto bufferCountMax = m_stream->NumBuffersAnnouncedMinRequired();
    bufferCountMax = bufferCountMax > 5 ? bufferCountMax : 5;

    // Allocate and announce image buffers and queue them
    for (size_t bufferCount = 0; bufferCount < bufferCountMax; ++bufferCount)
    {
        auto buffer = m_stream->AllocAndAnnounceBuffer(static_cast<size_t>(payloadSize), nullptr);
        m_stream->QueueBuffer(buffer);
    }
}

void IDSCamera::reset_params()
{
    if (!m_nodemap) throw "no nodemap";

    // Single frame acquisition configurations
    m_nodemap->FindNode<peak::core::nodes::EnumerationNode>("AcquisitionMode")->SetCurrentEntry("SingleFrame");

    // Here without ExposureStart trigger, replace it by software or hardware trigger if necessary
    m_nodemap->FindNode<peak::core::nodes::EnumerationNode>("TriggerSelector")->SetCurrentEntry("ExposureStart");
    m_nodemap->FindNode<peak::core::nodes::EnumerationNode>("TriggerMode")->SetCurrentEntry("Off");

    // disable auto-exposure/gain adjustment
    try {
         m_nodemap->FindNode<peak::core::nodes::EnumerationNode>("ExposureAuto")->SetCurrentEntry("Off");
    } catch (std::exception &e){
        //fprintf(stderr, "IDSCamera: %s\n", e.what());
    }

    try {
        m_nodemap->FindNode<peak::core::nodes::EnumerationNode>("GainAuto")->SetCurrentEntry("Off");
    } catch (std::exception &e){
        //fprintf(stderr, "IDSCamera: %s\n", e.what());
    }

    if (m_exposure > 0)
    {
        m_nodemap->FindNode<peak::core::nodes::FloatNode>("ExposureTime")->SetValue(m_exposure);
    }
    else
    {
        // set minimum exposure time
        double minExposureTime =m_nodemap->FindNode<peak::core::nodes::FloatNode>("ExposureTime")->Minimum();
        m_nodemap->FindNode<peak::core::nodes::FloatNode>("ExposureTime")->SetValue(minExposureTime);
    }

    double min_gain = m_nodemap->FindNode<peak::core::nodes::FloatNode>("Gain")->Minimum();
    m_nodemap->FindNode<peak::core::nodes::FloatNode>("Gain")->SetValue(min_gain);
}

void IDSCamera::open_device()
{
    static const char *prefix = "IDSCamera:";
    // Create instance of the device manager
    auto& deviceManager = peak::DeviceManager::Instance();

    // Update the device manager
    deviceManager.Update();

    // Return if no device was found
    if (deviceManager.Devices().empty())
        throw "no devices found";

    // open the first openable device in the device manager's device list
    size_t deviceCount = deviceManager.Devices().size();
    for (size_t i = 0; i < deviceCount; ++i)
    {
        if (deviceManager.Devices().at(i)->IsOpenable())
        {
            m_device = deviceManager.Devices().at(i)->OpenDevice(peak::core::DeviceAccessType::Control);

            // stop after the first opened device
            break;
        }
        else if (i == (deviceCount - 1))
        {
            char msg[50];
            snprintf(msg, 50, "could not open %lu devices\n", deviceCount);
            throw msg;
        }
    }
}



