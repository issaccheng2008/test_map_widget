#ifndef OVERLAYIMAGEWIDGET_H
#define OVERLAYIMAGEWIDGET_H

#include <QGraphicsView>
#include <QPolygonF>
#include <QString>
#include <QPixmap>

class QGraphicsPixmapItem;
class QGraphicsItem;

class RotationHandle;

class OverlayImageWidget : public QGraphicsView
{
    Q_OBJECT
public:
    explicit OverlayImageWidget(QWidget *parent = nullptr);

    [[nodiscard]] bool loadImage(const QString &filePath);
    void clearImage();
    [[nodiscard]] bool hasImage() const;
    [[nodiscard]] QPolygonF currentImageViewportPolygon() const;
    void setPinnedMode(bool pinned);
    [[nodiscard]] bool isPinned() const { return m_isPinned; }
    void applyViewportPolygon(const QPolygonF &viewportPolygon);
    [[nodiscard]] QPixmap currentPixmap() const;
    void setCurrentPixmap(const QPixmap &pixmap);

signals:
    void interactiveTransformChanged();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    friend class RotationHandle;

    void updateTransform();
    void updateMouseTransparency();
    void beginRotation(const QPointF &scenePos);
    void updateRotationFromScenePos(const QPointF &scenePos);
    void endRotation();

    QGraphicsScene *m_scene = nullptr;
    QGraphicsPixmapItem *m_pixmapItem = nullptr;
    QGraphicsItem *m_rotationHandle = nullptr;
    QPointF m_lastMousePosition;
    bool m_isDragging = false;
    bool m_isRotating = false;
    qreal m_currentScale = 1.0;
    qreal m_currentRotation = 0.0;
    qreal m_rotationInitial = 0.0;
    qreal m_rotationStartAngle = 0.0;
    QPointF m_rotationCenter;
    bool m_isPinned = false;
    qreal m_savedScaleBeforePin = 1.0;
    qreal m_savedRotationBeforePin = 0.0;

    QPixmap m_fullResolutionPixmap;
};

#endif // OVERLAYIMAGEWIDGET_H
