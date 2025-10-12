#ifndef OVERLAYIMAGEWIDGET_H
#define OVERLAYIMAGEWIDGET_H

#include <QGraphicsView>
#include <QString>

class QGraphicsPixmapItem;

class OverlayImageWidget : public QGraphicsView
{
    Q_OBJECT
public:
    explicit OverlayImageWidget(QWidget *parent = nullptr);

    [[nodiscard]] bool loadImage(const QString &filePath);
    void clearImage();
    [[nodiscard]] bool hasImage() const;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void updateTransform();
    void updateMouseTransparency();

    QGraphicsScene *m_scene = nullptr;
    QGraphicsPixmapItem *m_pixmapItem = nullptr;
    QPointF m_lastMousePosition;
    bool m_isDragging = false;
    qreal m_currentScale = 1.0;
    qreal m_currentRotation = 0.0;
};

#endif // OVERLAYIMAGEWIDGET_H
