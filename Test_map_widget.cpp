// Copyright 2025 ESRI
//
// All rights reserved under the copyright laws of the United States
// and applicable international laws, treaties, and conventions.
//
// You may freely redistribute and use this sample code, with or
// without modification, provided you include the original copyright
// notice and use restrictions.
//
// See the Sample code usage restrictions document for further information.
//

// Other headers
#include "Test_map_widget.h"

#include "OverlayImageWidget.h"

// Qt headers
#include <QColor>
#include <QFileDialog>
#include <QFileInfo>
#include <QFuture>
#include <QLineEdit>
#include <QPushButton>
#include <QPolygonF>
#include <QRegularExpression>
#include <QStatusBar>
#include <QStringList>
#include <QEvent>
#include <QMouseEvent>

// Standard library
#include <cmath>

// C++ API headers
#include "Geometry.h"
#include "GeometryEngine.h"
#include "Graphic.h"
#include "GraphicsOverlay.h"
#include "Map.h"
#include "MapGraphicsView.h"
#include "MapTypes.h"
#include "Point.h"
#include "PolygonBuilder.h"
#include "SimpleFillSymbol.h"
#include "SimpleLineSymbol.h"
#include "SpatialReference.h"
#include "SimpleMarkerSymbol.h"
#include "SymbolTypes.h"
#include "PolylineBuilder.h"
#include "GraphicListModel.h"
#include "GraphicsOverlayListModel.h"

#include "ui_Test_map_widget.h"

QList<obstacles> obstaclesList;

using namespace Esri::ArcGISRuntime;

Test_map_widget::Test_map_widget(QWidget *parent /*=nullptr*/)
    : QMainWindow(parent)
    , ui(new Ui::Test_map_widget)
{
    ui->setupUi(this);

    // Create a map using the ArcGISImagery BasemapStyle
    m_map = new Map(BasemapStyle::ArcGISImagery, this);

    // Create the map view widget
    m_mapView = ui->mapView;

    // Create the image overlay widget that sits on top of the map view
    m_imageOverlay = new OverlayImageWidget(m_mapView);
    m_imageOverlay->setObjectName(QStringLiteral("imageOverlay"));
    m_imageOverlay->setGeometry(m_mapView->rect());
    m_mapView->installEventFilter(this);

    // Set map to map view
    m_mapView->setMap(m_map);

    // Prepare a graphics overlay for displaying dynamic shapes
    m_graphicsOverlay = new GraphicsOverlay(this);
    m_mapView->graphicsOverlays()->append(m_graphicsOverlay);

    m_obstacleOverlay = new GraphicsOverlay(this);
    m_mapView->graphicsOverlays()->append(m_obstacleOverlay);

    m_obstacleEditingOverlay = new GraphicsOverlay(this);
    m_mapView->graphicsOverlays()->append(m_obstacleEditingOverlay);

    ui->finishObstacleButton->setEnabled(false);

    connect(ui->goToCoordinateButton, &QPushButton::clicked, this, &Test_map_widget::goToCoordinates);
    connect(ui->importImageButton, &QPushButton::clicked, this, &Test_map_widget::importImage);
    connect(ui->removeImageButton, &QPushButton::clicked, this, &Test_map_widget::clearImportedImage);
    connect(ui->setPositionButton, &QPushButton::clicked, this, &Test_map_widget::setImagePosition);
    connect(ui->addObstacleButton, &QPushButton::clicked, this, &Test_map_widget::startObstacleCapture);
    connect(ui->finishObstacleButton, &QPushButton::clicked, this, &Test_map_widget::finishObstacleCapture);
    connect(ui->cancelObstacleButton, &QPushButton::clicked, this, &Test_map_widget::cancelObstacleCapture);
    // Connect the exit button created in the UI to close the window
    connect(ui->exitButton, &QPushButton::clicked, this, &QWidget::close);
}

Test_map_widget::~Test_map_widget()
{
    delete ui;
}

void Test_map_widget::goToCoordinates()
{
    if (!m_mapView)
        return;

    const QString coordinateText = ui->coordinateInput->text().trimmed();
    if (coordinateText.isEmpty()) {
        statusBar()->showMessage(tr("Enter coordinates as latitude, longitude."), 5000);
        return;
    }

    QString normalized = coordinateText;
    normalized.replace(QLatin1Char(','), QLatin1Char(' '));
    const QStringList parts = normalized.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    if (parts.size() != 2) {
        statusBar()->showMessage(tr("Enter coordinates as latitude, longitude."), 5000);
        return;
    }

    bool latOk = false;
    const double latitude = parts.at(0).toDouble(&latOk);
    bool lonOk = false;
    const double longitude = parts.at(1).toDouble(&lonOk);
    if (!latOk || !lonOk || latitude < -90.0 || latitude > 90.0 || longitude < -180.0 || longitude > 180.0) {
        statusBar()->showMessage(tr("Coordinates must be valid latitude and longitude."), 5000);
        return;
    }

    const Point location(longitude, latitude, SpatialReference::wgs84());
    constexpr double zoomScale = 10000.0;
    m_mapView->setViewpointCenterAsync(location, zoomScale);
    statusBar()->showMessage(tr("Zoomed to %1, %2").arg(latitude, 0, 'f', 4).arg(longitude, 0, 'f', 4), 5000);

    const Point endlocation(longitude,latitude+1,SpatialReference::wgs84());

    drawLineBetweenCoordinates(location,endlocation);

}

void Test_map_widget::drawLineBetweenCoordinates(const Point &start, const Point &end)
{
    if (!m_graphicsOverlay)
        return;

    const SpatialReference wgs84 = SpatialReference::wgs84();
    const SpatialReference webMercator = SpatialReference::webMercator();

    const auto normalizeToWgs84 = [&wgs84](const Point &point) {
        if (point.spatialReference().isEmpty() || point.spatialReference() == wgs84)
            return Point(point.x(), point.y(), wgs84);
        return geometry_cast<Point>(GeometryEngine::project(point, wgs84));
    };

    const Point startWgs84 = normalizeToWgs84(start);
    const Point endWgs84 = normalizeToWgs84(end);

    const Point startWeb = geometry_cast<Point>(GeometryEngine::project(startWgs84, webMercator));
    const Point endWeb = geometry_cast<Point>(GeometryEngine::project(endWgs84, webMercator));

    const double dx = endWeb.x() - startWeb.x();
    const double dy = endWeb.y() - startWeb.y();
    const double length = std::hypot(dx, dy);
    if (length == 0.0)
        return;

    constexpr double halfWidthMeters = 0.75; // Half of the 1.5 meter width
    const double perpX = (-dy / length) * halfWidthMeters;
    const double perpY = (dx / length) * halfWidthMeters;

    const Point startTop(startWeb.x() + perpX, startWeb.y() + perpY, webMercator);
    const Point endTop(endWeb.x() + perpX, endWeb.y() + perpY, webMercator);
    const Point endBottom(endWeb.x() - perpX, endWeb.y() - perpY, webMercator);
    const Point startBottom(startWeb.x() - perpX, startWeb.y() - perpY, webMercator);

    PolygonBuilder builder(webMercator);
    builder.addPoint(startTop);
    builder.addPoint(endTop);
    builder.addPoint(endBottom);
    builder.addPoint(startBottom);

    const Geometry rectangleWebMercator = builder.toGeometry();
    const Geometry rectangleWgs84 = GeometryEngine::project(rectangleWebMercator, wgs84);

    const QColor fillColor(0, 0, 255, 127);
    auto *outlineSymbol = new SimpleLineSymbol(SimpleLineSymbolStyle::Solid, fillColor, 0.0f, this);
    auto *fillSymbol = new SimpleFillSymbol(SimpleFillSymbolStyle::Solid, fillColor, outlineSymbol, this);

    auto *rectangleGraphic = new Graphic(rectangleWgs84, fillSymbol, this);
    m_graphicsOverlay->graphics()->append(rectangleGraphic);
}

bool Test_map_widget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_mapView) {
        if (event->type() == QEvent::Resize) {
            if (m_imageOverlay)
                m_imageOverlay->setGeometry(m_mapView->rect());
        } else if (event->type() == QEvent::MouseButtonPress && m_isCapturingObstacle) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::RightButton) {
                addObstaclePoint(mouseEvent->pos());
                return true;
            }
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void Test_map_widget::importImage()
{
    if (!m_imageOverlay)
        return;

    const QString filePath = QFileDialog::getOpenFileName(this,
                                                         tr("Import image"),
                                                         QString(),
                                                         tr("Image Files (*.png *.jpg *.jpeg *.bmp *.gif *.tif *.tiff);;All Files (*)"));
    if (filePath.isEmpty())
        return;

    if (m_imageOverlay->loadImage(filePath)) {
        const QFileInfo info(filePath);
        statusBar()->showMessage(tr("Loaded %1. Drag to move, use the mouse wheel to zoom, and hold Shift while using the wheel to rotate.")
                                     .arg(info.fileName()),
                                 8000);
    } else {
        statusBar()->showMessage(tr("Failed to load image."), 5000);
    }
}

void Test_map_widget::clearImportedImage()
{
    if (!m_imageOverlay)
        return;

    if (m_imageOverlay->hasImage()) {
        m_imageOverlay->clearImage();
        statusBar()->showMessage(tr("Image removed."), 5000);
    }
}

void Test_map_widget::setImagePosition()
{
    if (!m_imageOverlay || !m_mapView || !m_graphicsOverlay)
        return;

    if (!m_imageOverlay->hasImage()) {
        statusBar()->showMessage(tr("Import an image before setting its position."), 5000);
        return;
    }

    const QPolygonF viewportPolygon = m_imageOverlay->currentImageViewportPolygon();
    if (viewportPolygon.size() < 3) {
        statusBar()->showMessage(tr("Unable to determine the image footprint."), 5000);
        return;
    }

    QList<Point> mapPoints;
    mapPoints.reserve(viewportPolygon.size());

    const auto pointsApproximatelyEqual = [](const Point &a, const Point &b) {
        constexpr double tolerance = 1e-6;
        return std::abs(a.x() - b.x()) < tolerance && std::abs(a.y() - b.y()) < tolerance;
    };

    for (const QPointF &screenPointF : viewportPolygon) {
        const Point mapPoint = m_mapView->screenToLocation(screenPointF.x(), screenPointF.y());
        if (!mapPoints.isEmpty() && pointsApproximatelyEqual(mapPoints.constLast(), mapPoint))
            continue;
        mapPoints.append(mapPoint);
    }

    if (mapPoints.size() < 3) {
        statusBar()->showMessage(tr("Unable to determine the image footprint."), 5000);
        return;
    }

    PolygonBuilder builder(mapPoints.first().spatialReference());
    for (const Point &point : mapPoints)
        builder.addPoint(point);

    if (!pointsApproximatelyEqual(mapPoints.first(), mapPoints.last()))
        builder.addPoint(mapPoints.first());

    const QColor fillColor(0, 122, 204, 90);
    auto *fillSymbol = new SimpleFillSymbol(SimpleFillSymbolStyle::Solid, fillColor, nullptr, this);
    auto *footprintGraphic = new Graphic(builder.toGeometry(), fillSymbol, this);
    m_graphicsOverlay->graphics()->append(footprintGraphic);

    m_imageOverlay->clearImage();
    statusBar()->showMessage(tr("Image replaced with map footprint."), 5000);
}

void Test_map_widget::startObstacleCapture()
{
    resetObstacleCreationState(true);
    m_isCapturingObstacle = true;

    if (statusBar())
        statusBar()->showMessage(tr("Right-click on the map to add obstacle vertices."), 5000);
}

void Test_map_widget::finishObstacleCapture()
{
    if (!m_isCapturingObstacle || m_currentObstaclePoints.size() < 3 || !m_obstacleOverlay)
        return;

    obstacles newObstacle;
    newObstacle.vertices = m_currentObstaclePoints;
    obstaclesList.append(newObstacle);

    PolygonBuilder builder(m_currentObstaclePoints.first().spatialReference());
    for (const auto &point : m_currentObstaclePoints)
        builder.addPoint(point);

    const QColor fillColor(255, 0, 0, 100);
    auto *fillSymbol = new SimpleFillSymbol(SimpleFillSymbolStyle::Solid, fillColor, nullptr, this);
    auto *polygonGraphic = new Graphic(builder.toGeometry(), fillSymbol, this);
    m_obstacleOverlay->graphics()->append(polygonGraphic);

    resetObstacleCreationState(false);

    if (statusBar())
        statusBar()->showMessage(tr("Obstacle saved."), 5000);
}

void Test_map_widget::cancelObstacleCapture()
{
    if (!m_isCapturingObstacle && m_currentObstaclePoints.isEmpty())
        return;

    resetObstacleCreationState(false);

    if (statusBar())
        statusBar()->showMessage(tr("Obstacle creation canceled."), 5000);
}

void Test_map_widget::addObstaclePoint(const QPoint &screenPoint)
{
    if (!m_isCapturingObstacle || !m_mapView)
        return;

    const Point mapPoint = m_mapView->screenToLocation(screenPoint.x(),screenPoint.y());
    m_currentObstaclePoints.append(mapPoint);

    rebuildObstaclePreview();

    if (ui && ui->finishObstacleButton)
        ui->finishObstacleButton->setEnabled(m_currentObstaclePoints.size() >= 3);
}

void Test_map_widget::rebuildObstaclePreview()
{
    if (!m_obstacleEditingOverlay)
        return;

    auto *graphicsModel = m_obstacleEditingOverlay->graphics();
    graphicsModel->clear();

    const QColor markerColor(255, 0, 0);
    for (const auto &point : m_currentObstaclePoints) {
        auto *markerSymbol = new SimpleMarkerSymbol(SimpleMarkerSymbolStyle::Circle, markerColor, 8.0f, this);
        graphicsModel->append(new Graphic(point, markerSymbol, this));
    }

    if (m_currentObstaclePoints.size() < 3)
        return;

    PolygonBuilder builder(m_currentObstaclePoints.first().spatialReference());
    for (const auto &point : m_currentObstaclePoints)
        builder.addPoint(point);

    const QColor fillColor(255, 0, 0, 80);
    auto *fillSymbol = new SimpleFillSymbol(SimpleFillSymbolStyle::Solid, fillColor, nullptr, this);
    graphicsModel->append(new Graphic(builder.toGeometry(), fillSymbol, this));
}

void Test_map_widget::resetObstacleCreationState(bool keepActive)
{
    if (!keepActive)
        m_isCapturingObstacle = false;

    m_currentObstaclePoints.clear();

    if (m_obstacleEditingOverlay)
        m_obstacleEditingOverlay->graphics()->clear();

    if (ui && ui->finishObstacleButton)
        ui->finishObstacleButton->setEnabled(false);
}
