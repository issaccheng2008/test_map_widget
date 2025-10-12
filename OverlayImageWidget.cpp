#include "OverlayImageWidget.h"

#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace
{
constexpr qreal kMinimumScale = 0.1;
constexpr qreal kMaximumScale = 10.0;
constexpr qreal kRotationStepDegrees = 5.0;
constexpr qreal kSceneExtent = 10000.0;
}

OverlayImageWidget::OverlayImageWidget(QWidget *parent)
    : QGraphicsView(parent)
    , m_scene(new QGraphicsScene(this))
{
    setScene(m_scene);
    setFrameShape(QFrame::NoFrame);
    setStyleSheet(QStringLiteral("background: transparent;"));
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);
    setSceneRect(QRectF(-kSceneExtent, -kSceneExtent, kSceneExtent * 2.0, kSceneExtent * 2.0));

    updateMouseTransparency();
}

bool OverlayImageWidget::loadImage(const QString &filePath)
{
    QPixmap pixmap(filePath);
    if (pixmap.isNull())
        return false;

    clearImage();

    m_pixmapItem = m_scene->addPixmap(pixmap);
    m_pixmapItem->setTransformationMode(Qt::SmoothTransformation);
    m_pixmapItem->setTransformOriginPoint(m_pixmapItem->boundingRect().center());

    m_currentScale = 1.0;
    m_currentRotation = 0.0;
    updateTransform();

    const QPointF viewCenter = mapToScene(viewport()->rect().center());
    const QPointF itemCenter = m_pixmapItem->boundingRect().center();
    m_pixmapItem->setPos(viewCenter - itemCenter);

    updateMouseTransparency();
    viewport()->update();
    show();
    return true;
}

void OverlayImageWidget::clearImage()
{
    if (m_pixmapItem) {
        m_scene->removeItem(m_pixmapItem);
        delete m_pixmapItem;
        m_pixmapItem = nullptr;
    }
    m_currentScale = 1.0;
    m_currentRotation = 0.0;
    m_isDragging = false;
    updateMouseTransparency();
    viewport()->update();
}

bool OverlayImageWidget::hasImage() const
{
    return m_pixmapItem != nullptr;
}

void OverlayImageWidget::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
}

void OverlayImageWidget::wheelEvent(QWheelEvent *event)
{
    if (!m_pixmapItem) {
        event->ignore();
        return;
    }

    if (event->modifiers() & Qt::ShiftModifier) {
        const qreal degrees = (event->angleDelta().y() / 120.0) * kRotationStepDegrees;
        m_currentRotation += degrees;
    } else {
        const qreal factor = std::pow(1.0015, event->angleDelta().y());
        m_currentScale = std::clamp(m_currentScale * factor, kMinimumScale, kMaximumScale);
    }

    updateTransform();
    event->accept();
}

void OverlayImageWidget::mousePressEvent(QMouseEvent *event)
{
    if (!m_pixmapItem || event->button() != Qt::LeftButton) {
        QGraphicsView::mousePressEvent(event);
        return;
    }

    m_isDragging = true;
    m_lastMousePosition = mapToScene(event->pos());
    event->accept();
}

void OverlayImageWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_pixmapItem || !m_isDragging) {
        QGraphicsView::mouseMoveEvent(event);
        return;
    }

    const QPointF currentPosition = mapToScene(event->pos());
    const QPointF delta = currentPosition - m_lastMousePosition;
    m_pixmapItem->setPos(m_pixmapItem->pos() + delta);
    m_lastMousePosition = currentPosition;
    event->accept();
}

void OverlayImageWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_isDragging) {
        m_isDragging = false;
        event->accept();
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void OverlayImageWidget::updateTransform()
{
    if (!m_pixmapItem)
        return;

    m_pixmapItem->setScale(m_currentScale);
    m_pixmapItem->setRotation(m_currentRotation);
}

void OverlayImageWidget::updateMouseTransparency()
{
    const bool transparent = !hasImage();
    setAttribute(Qt::WA_TransparentForMouseEvents, transparent);
    setVisible(!transparent);
}
