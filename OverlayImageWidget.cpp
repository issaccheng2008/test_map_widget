#include "OverlayImageWidget.h"

#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRegion>
#include <QWheelEvent>
#include <QCursor>
#include <QFont>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
constexpr qreal kMinimumScale = 0.1;
constexpr qreal kMaximumScale = 10.0;
constexpr qreal kRotationStepDegrees = 5.0;
constexpr qreal kSceneExtent = 10000.0;
constexpr qreal kHandleOffset = 30.0;
constexpr qreal kHandleRadius = 14.0;
constexpr qreal kHandleHoverRadius = 18.0;
}

class RotationHandle : public QGraphicsItem
{
public:
    explicit RotationHandle(OverlayImageWidget *owner, QGraphicsItem *parent = nullptr)
        : QGraphicsItem(parent)
        , m_owner(owner)
    {
        setAcceptedMouseButtons(Qt::LeftButton);
        setAcceptHoverEvents(true);
        setCursor(Qt::OpenHandCursor);
        setZValue(1.0);
    }

    QRectF boundingRect() const override
    {
        const qreal diameter = kHandleHoverRadius * 2.0;
        return QRectF(-kHandleHoverRadius, -kHandleHoverRadius, diameter, diameter);
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override
    {
        painter->setRenderHint(QPainter::Antialiasing, true);
        const qreal radius = m_hovered ? kHandleHoverRadius : kHandleRadius;
        painter->setPen(QPen(QColor(40, 40, 40), 1.5));
        painter->setBrush(QColor(255, 255, 255, 230));
        painter->drawEllipse(QPointF(0, 0), radius, radius);

        QFont font = painter->font();
        font.setPointSizeF(radius * 0.9);
        painter->setFont(font);
        painter->setPen(QPen(QColor(40, 40, 40)));
        painter->drawText(QRectF(-radius, -radius, radius * 2.0, radius * 2.0), Qt::AlignCenter, QStringLiteral("↻"));
    }

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override
    {
        m_hovered = true;
        update();
        QGraphicsItem::hoverEnterEvent(event);
    }

    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override
    {
        m_hovered = false;
        update();
        QGraphicsItem::hoverLeaveEvent(event);
    }

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override
    {
        if (!m_owner) {
            event->ignore();
            return;
        }
        m_dragging = true;
        setCursor(Qt::ClosedHandCursor);
        m_owner->beginRotation(event->scenePos());
        event->accept();
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override
    {
        if (!m_owner || !m_dragging) {
            event->ignore();
            return;
        }
        m_owner->updateRotationFromScenePos(event->scenePos());
        event->accept();
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override
    {
        if (!m_owner || !m_dragging) {
            event->ignore();
            return;
        }
        m_dragging = false;
        setCursor(Qt::OpenHandCursor);
        m_owner->endRotation();
        event->accept();
    }

private:
    OverlayImageWidget *m_owner = nullptr;
    bool m_hovered = false;
    bool m_dragging = false;
};

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
    m_pixmapItem->setAcceptedMouseButtons(Qt::NoButton);

    if (m_rotationHandle) {
        delete m_rotationHandle;
        m_rotationHandle = nullptr;
    }
    m_rotationHandle = new RotationHandle(this, m_pixmapItem);
    const QRectF rect = m_pixmapItem->boundingRect();
    m_rotationHandle->setPos(rect.center().x(), rect.top() - kHandleOffset);

    m_currentScale = 1.0;
    m_currentRotation = 0.0;
    updateTransform();

    const QPointF viewCenter = mapToScene(viewport()->rect().center());
    const QPointF itemCenter = m_pixmapItem->boundingRect().center();
    m_pixmapItem->setPos(viewCenter - itemCenter);

    updateInteractionRegion();

    updateMouseTransparency();
    viewport()->update();
    show();
    return true;
}

void OverlayImageWidget::clearImage()
{
    if (m_pixmapItem) {
        if (m_rotationHandle) {
            delete m_rotationHandle;
            m_rotationHandle = nullptr;
        }
        m_scene->removeItem(m_pixmapItem);
        delete m_pixmapItem;
        m_pixmapItem = nullptr;
    }
    setMask(QRegion());
    m_currentScale = 1.0;
    m_currentRotation = 0.0;
    m_isDragging = false;
    m_isRotating = false;
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
    if (!m_pixmapItem || m_isRotating) {
        QGraphicsView::mousePressEvent(event);
        return;
    }

    QGraphicsView::mousePressEvent(event);
    if (event->isAccepted())
        return;

    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }

    const QPointF scenePos = mapToScene(event->pos());
    const QPointF itemPos = m_pixmapItem->mapFromScene(scenePos);
    if (!m_pixmapItem->contains(itemPos)) {
        event->ignore();
        return;
    }

    m_isDragging = true;
    m_lastMousePosition = scenePos;
    event->accept();
}

void OverlayImageWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_pixmapItem || m_isRotating) {
        QGraphicsView::mouseMoveEvent(event);
        return;
    }

    if (!m_isDragging) {
        QGraphicsView::mouseMoveEvent(event);
        return;
    }

    const QPointF currentPosition = mapToScene(event->pos());
    const QPointF delta = currentPosition - m_lastMousePosition;
    m_pixmapItem->setPos(m_pixmapItem->pos() + delta);
    m_lastMousePosition = currentPosition;
    updateInteractionRegion();
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
    updateInteractionRegion();
}

void OverlayImageWidget::updateMouseTransparency()
{
    const bool transparent = !hasImage();
    setAttribute(Qt::WA_TransparentForMouseEvents, transparent);
    setVisible(!transparent);
    if (transparent) {
        setMask(QRegion());
    }
}

void OverlayImageWidget::beginRotation(const QPointF &scenePos)
{
    if (!m_pixmapItem)
        return;

    m_isRotating = true;
    m_isDragging = false;
    m_rotationInitial = m_currentRotation;
    m_rotationCenter = m_pixmapItem->sceneBoundingRect().center();
    const QPointF vector = scenePos - m_rotationCenter;
    m_rotationStartAngle = std::atan2(vector.y(), vector.x());
}

void OverlayImageWidget::updateRotationFromScenePos(const QPointF &scenePos)
{
    if (!m_isRotating || !m_pixmapItem)
        return;

    const QPointF vector = scenePos - m_rotationCenter;
    if (vector.manhattanLength() < std::numeric_limits<qreal>::epsilon())
        return;

    const qreal angle = std::atan2(vector.y(), vector.x());
    const qreal delta = angle - m_rotationStartAngle;
    m_currentRotation = m_rotationInitial + qRadiansToDegrees(delta);
    updateTransform();
}

void OverlayImageWidget::endRotation()
{
    m_isRotating = false;
}

void OverlayImageWidget::updateInteractionRegion()
{
    if (!m_pixmapItem) {
        setMask(QRegion());
        return;
    }

    QPainterPath path;

    const QPolygonF pixmapScene = m_pixmapItem->mapToScene(m_pixmapItem->boundingRect());
    const QPolygon pixmapView = mapFromScene(pixmapScene);
    if (!pixmapView.isEmpty())
        path.addPolygon(pixmapView);

    if (m_rotationHandle) {
        const QPolygonF handleScene = m_rotationHandle->mapToScene(m_rotationHandle->boundingRect());
        const QPolygon handleView = mapFromScene(handleScene);
        if (!handleView.isEmpty())
            path.addPolygon(handleView);
    }

    if (path.isEmpty()) {
        setMask(QRegion());
        return;
    }

    const QRegion region(path.toFillPolygon().toPolygon());
    setMask(region);
}
