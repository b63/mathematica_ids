
#include <QGraphicsView>
#include <QImage>
#include <QWidget>

#include <cmath>

#include "Display.h"


ImageDisplay::ImageDisplay(QWidget* parent)
    : QGraphicsView(parent), 
      m_mode(NO_FILL)
{
    m_scene = new CustomGraphicsScene(this);
    this->setScene(m_scene);
}


ImageDisplay::~ImageDisplay()
{}

FillMode ImageDisplay::get_display_mode()
{
    return m_mode;
}

void ImageDisplay::set_display_mode(FillMode mode)
{
    m_mode = mode;
    m_scene->update();
}

void ImageDisplay::onImageReceived(QImage image)
{
    m_scene->setImage(image);
}


CustomGraphicsScene::CustomGraphicsScene(ImageDisplay* parent)
    : QGraphicsScene(parent),
    m_parent(parent)
{}


CustomGraphicsScene::~CustomGraphicsScene()
{}


void CustomGraphicsScene::setImage(QImage image)
{
    m_image = image;
    update();
}


void CustomGraphicsScene::drawBackground(QPainter* painter, const QRectF&)
{
    // Display size
    double displayWidth = static_cast<double>(m_parent->width());
    double displayHeight = static_cast<double>(m_parent->height());

    // Image size
    double imageWidth = static_cast<double>(m_image.width());
    double imageHeight = static_cast<double>(m_image.height());

    if (m_parent->get_display_mode() == FILL_FIT)
    {
        // Calculate aspect ratio of the display
        double ratio1 = displayWidth / displayHeight;

        // Calculate aspect ratio of the image
        double ratio2 = imageWidth / imageHeight;

        if (ratio1 > ratio2)
        {
            // the height with must fit to the display height. So h remains and w must be scaled down
            imageWidth = displayHeight * ratio2;
            imageHeight = displayHeight;
        }
        else
        {
            // the image with must fit to the display width. So w remains and h must be scaled down
            imageWidth = displayWidth;
            imageHeight = displayWidth / ratio2;
        }
    }

    double imagePosX = -1.0 * (imageWidth / 2.0);
    double imagePosY = -1.0 * (imageHeight / 2.0);

    // Remove digits afer point
    imagePosX = trunc(imagePosX);
    imagePosY = trunc(imagePosY);

    QRectF rect(imagePosX, imagePosY, imageWidth, imageHeight);

    painter->fillRect(-displayWidth/2, -displayHeight/2, displayWidth, displayHeight, QColor::fromRgb(0,0,0));
    painter->drawImage(rect, m_image);
}
