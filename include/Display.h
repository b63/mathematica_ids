#ifndef DISPLAY_H
#define DISPLAY_H

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPainter>
#include <QRect>
#include <QObject>

#include <cstdint>


class ImageDisplay;

class CustomGraphicsScene : public QGraphicsScene
{
    Q_OBJECT

public:
    CustomGraphicsScene(ImageDisplay* pParent);
    ~CustomGraphicsScene();

    void setImage(QImage image);

private:
    ImageDisplay* m_parent;
    QImage m_image;

    virtual void drawBackground(QPainter* painter, const QRectF& rect);
};


enum FillMode {NO_FILL, FILL_FIT};

class ImageDisplay : public QGraphicsView
{
    Q_OBJECT

public:
    ImageDisplay(QWidget* parent);
    ~ImageDisplay();
    void set_display_mode(FillMode);
    FillMode get_display_mode();

private:
    CustomGraphicsScene* m_scene;
    FillMode m_mode;


public slots:
    void onImageReceived(QImage image);
};

#endif // DISPLAY_H
