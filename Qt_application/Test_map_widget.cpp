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
#include "GridPreviewWindow.h"
#include "GridState.h"
#include "generate_path.h"
#include "network.h"

// Qt headers
#include <QAbstractScrollArea>
#include <QColor>
#include <QFileDialog>
#include <QFileInfo>
#include <QFuture>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QPolygonF>
#include <QRegularExpression>
#include <QStatusBar>
#include <QStringList>
#include <QSignalBlocker>
#include <QEvent>
#include <QMouseEvent>
#include <QWidget>
#include <QVector2D>
#include <QUrl>

// Standard library
#include <algorithm>
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
QVector<QVector<int>> g_channelGrid;
QPolygonF g_pinnedImageFootprint;

using namespace Esri::ArcGISRuntime;

Test_map_widget::Test_map_widget(QWidget *parent /*=nullptr*/)
    : QMainWindow(parent)
    , ui(new Ui::Test_map_widget)
{
    ui->setupUi(this);

    if (ui->sideScrollArea)
        ui->sideScrollArea->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);

    // Create a map using the ArcGISImagery BasemapStyle
    m_map = new Map(BasemapStyle::ArcGISImagery, this);

    // Create the map view widget
    m_mapView = ui->mapView;
    m_mapView->setMouseTracking(true);

    if (QWidget *viewport = m_mapView->viewport()) {
        viewport->setMouseTracking(true);
        viewport->installEventFilter(this);
    }

    // Create the image overlay widget that sits on top of the map view
    m_imageOverlay = new OverlayImageWidget(m_mapView);
    m_imageOverlay->setObjectName(QStringLiteral("imageOverlay"));
    m_imageOverlay->setGeometry(m_mapView->rect());
    m_mapView->installEventFilter(this);

    connect(m_imageOverlay, &OverlayImageWidget::interactiveTransformChanged, this, &Test_map_widget::updateUiState);

    // Set map to map view
    m_mapView->setMap(m_map);

    connect(m_mapView, &MapGraphicsView::viewpointChanged, this, [this](auto &&...) {
        updatePinnedImagePosition();
        updateUiState();
    });

    // Prepare a graphics overlay for displaying dynamic shapes
    m_graphicsOverlay = new GraphicsOverlay(this);
    m_mapView->graphicsOverlays()->append(m_graphicsOverlay);

    m_workAreaOverlay = new GraphicsOverlay(this);
    m_mapView->graphicsOverlays()->append(m_workAreaOverlay);

    m_obstacleOverlay = new GraphicsOverlay(this);
    m_mapView->graphicsOverlays()->append(m_obstacleOverlay);

    m_obstacleEditingOverlay = new GraphicsOverlay(this);
    m_mapView->graphicsOverlays()->append(m_obstacleEditingOverlay);

    m_locationOverlay = new GraphicsOverlay(this);
    m_mapView->graphicsOverlays()->append(m_locationOverlay);

    connect(ui->goToCoordinateButton, &QPushButton::clicked, this, &Test_map_widget::goToCoordinates);
    connect(ui->importImageButton, &QPushButton::clicked, this, &Test_map_widget::importImage);
    connect(ui->removeImageButton, &QPushButton::clicked, this, &Test_map_widget::clearImportedImage);
    connect(ui->setPositionButton, &QPushButton::clicked, this, &Test_map_widget::setImagePosition);
    connect(ui->openGridButton, &QPushButton::clicked, this, &Test_map_widget::openGridPreview);
    connect(ui->addObstacleButton, &QPushButton::clicked, this, &Test_map_widget::handleObstacleActionButton);
    connect(ui->cancelObstacleButton, &QPushButton::clicked, this, &Test_map_widget::cancelObstacleCapture);
    connect(ui->showWorkAreaButton, &QPushButton::clicked, this, &Test_map_widget::toggleWorkArea);
    connect(ui->generatePathButton, &QPushButton::clicked, this, &Test_map_widget::generatePathForCurrentImage);
    // Connect the exit button created in the UI to close the window
    connect(ui->exitButton, &QPushButton::clicked, this, &QWidget::close);

    m_gpsClient = new GpsNetworkClient(QUrl(QStringLiteral("http://192.168.43.95/gps")), this);
    connect(m_gpsClient, &GpsNetworkClient::coordinateReceived, this, &Test_map_widget::updateGpsCoordinate);
    connect(m_gpsClient, &GpsNetworkClient::networkError, this, &Test_map_widget::handleGpsError);
    m_gpsClient->start(2000);

    updateUiState();
}

Test_map_widget::~Test_map_widget()
{
    if (ui) {
        delete ui;
        ui = nullptr;
    }
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

    // const Point endlocation(longitude,latitude+1,SpatialReference::wgs84());

    // drawLineBetweenCoordinates(location,endlocation);

}

void Test_map_widget::updateGpsCoordinate(double latitude, double longitude)
{
    if (!ui || !ui->coordinateInput)
        return;

    const QLocale numberLocale = QLocale::c();
    const QString coordinateText = numberLocale.toString(latitude, 'f', 6) + QStringLiteral(", ") + numberLocale.toString(longitude, 'f', 6);
    {
        QSignalBlocker blocker(ui->coordinateInput);
        ui->coordinateInput->setText(coordinateText);
    }

    const Point location(longitude, latitude, SpatialReference::wgs84());

    if (m_locationOverlay) {
        if (!m_currentLocationGraphic) {
            const QColor markerColor(255, 69, 0); // Orange-red for visibility
            auto *symbol = new SimpleMarkerSymbol(SimpleMarkerSymbolStyle::Circle, markerColor, 12.0f, this);
            m_currentLocationGraphic = new Graphic(location, symbol, this);
            if (auto *graphics = m_locationOverlay->graphics())
                graphics->append(m_currentLocationGraphic);
        } else {
            m_currentLocationGraphic->setGeometry(location);
        }
    }

    if (!m_hasCenteredOnGps && m_mapView) {
        constexpr double zoomScale = 5000.0;
        m_mapView->setViewpointCenterAsync(location, zoomScale);
        m_hasCenteredOnGps = true;
    }
}

void Test_map_widget::handleGpsError(const QString &message)
{
    if (QStatusBar *bar = statusBar())
        bar->showMessage(message, 3000);
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

    const double halfWidthMeters = car_length / 2.0;
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
            updatePinnedImagePosition();
            updateUiState();
        }
        return QMainWindow::eventFilter(watched, event);
    }

    if (m_mapView && watched == m_mapView->viewport()) {
        switch (event->type()) {
        case QEvent::MouseMove: {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            updateCursorCoordinateDisplay(mouseEvent->pos());
            break;
        }
        case QEvent::MouseButtonPress: {
            if (m_isCapturingObstacle) {
                auto *mouseEvent = static_cast<QMouseEvent *>(event);
                if (mouseEvent->button() == Qt::RightButton) {
                    addObstaclePoint(mouseEvent->pos());
                    return true;
                }
            }
            break;
        }
        case QEvent::Leave:
            if (ui->cursorCoordinateValue)
                ui->cursorCoordinateValue->setText(tr("Lat: ---\nLon: ---"));
            break;
        default:
            break;
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void Test_map_widget::updateCursorCoordinateDisplay(const QPoint &screenPoint)
{
    if (!m_mapView || !ui->cursorCoordinateValue)
        return;

    const QString defaultText = tr("Lat: ---\nLon: ---");

    if (QWidget *viewport = m_mapView->viewport()) {
        if (!viewport->rect().contains(screenPoint)) {
            ui->cursorCoordinateValue->setText(defaultText);
            return;
        }
    }

    const Point mapPoint = m_mapView->screenToLocation(screenPoint.x(), screenPoint.y());
    if (mapPoint.isEmpty()) {
        ui->cursorCoordinateValue->setText(defaultText);
        return;
    }

    Point geographicPoint = mapPoint;
    const SpatialReference wgs84 = SpatialReference::wgs84();
    if (!geographicPoint.spatialReference().isEmpty() && geographicPoint.spatialReference() != wgs84) {
        const Geometry projected = GeometryEngine::project(mapPoint, wgs84);
        if (!projected.isEmpty())
            geographicPoint = geometry_cast<Point>(projected);
    }

    if (geographicPoint.isEmpty()) {
        ui->cursorCoordinateValue->setText(defaultText);
        return;
    }

    const double latitude = geographicPoint.y();
    const double longitude = geographicPoint.x();
    ui->cursorCoordinateValue->setText(tr("Lat: %1\nLon: %2").arg(latitude, 0, 'f', 6).arg(longitude, 0, 'f', 6));
}

void Test_map_widget::clearWorkAreaGraphic()
{
    if (m_workAreaOverlay) {
        if (auto *graphicsModel = m_workAreaOverlay->graphics())
            graphicsModel->clear();
    }

    m_workAreaGraphic = nullptr;
    m_cachedWorkArea.clear();
}

bool Test_map_widget::ensureWorkAreaGraphic()
{
    const auto polygonOptional = workAreaRectangle();
    if (!polygonOptional || polygonOptional->isEmpty())
        return false;

    if (!m_workAreaOverlay)
        return false;

    auto *graphicsModel = m_workAreaOverlay->graphics();
    if (!graphicsModel)
        return false;

    const QList<Point> polygonPoints = *polygonOptional;
    if (polygonPoints.isEmpty())
        return false;

    PolygonBuilder builder(polygonPoints.first().spatialReference());
    for (const Point &point : polygonPoints)
        builder.addPoint(point);
    builder.addPoint(polygonPoints.first());

    const Geometry geometry = builder.toGeometry();

    graphicsModel->clear();

    const QColor outlineColor(0, 255, 0, 200);
    auto *outlineSymbol = new SimpleLineSymbol(SimpleLineSymbolStyle::Solid, outlineColor, 1.5f, this);
    const QColor fillColor(0, 255, 0, 40);
    auto *fillSymbol = new SimpleFillSymbol(SimpleFillSymbolStyle::Solid, fillColor, outlineSymbol, this);

    m_workAreaGraphic = new Graphic(geometry, fillSymbol, this);
    graphicsModel->append(m_workAreaGraphic.data());

    m_cachedWorkArea = polygonPoints;
    return true;
}

std::optional<QList<Point>> Test_map_widget::workAreaRectangle() const
{
    if (!m_isImagePinned || m_pinnedImageMapPoints.size() < 4)
        return std::nullopt;

    const auto dimensions = pinnedImageDimensionsMeters();
    if (!dimensions)
        return std::nullopt;

    const SpatialReference webMercator = SpatialReference::webMercator();
    const SpatialReference wgs84 = SpatialReference::wgs84();

    const auto approximatelyEqual = [](const Point &a, const Point &b) {
        constexpr double tolerance = 1e-6;
        return std::abs(a.x() - b.x()) < tolerance && std::abs(a.y() - b.y()) < tolerance;
    };

    QList<Point> uniquePoints;
    uniquePoints.reserve(m_pinnedImageMapPoints.size());

    for (const Point &mapPoint : m_pinnedImageMapPoints) {
        Point projectedPoint = mapPoint;
        if (projectedPoint.spatialReference().isEmpty() || projectedPoint.spatialReference() != webMercator)
            projectedPoint = geometry_cast<Point>(GeometryEngine::project(mapPoint, webMercator));

        bool duplicate = false;
        for (const Point &existing : std::as_const(uniquePoints)) {
            if (approximatelyEqual(existing, projectedPoint)) {
                duplicate = true;
                break;
            }
        }

        if (!duplicate)
            uniquePoints.append(projectedPoint);

        if (uniquePoints.size() == 4)
            break;
    }

    if (uniquePoints.size() < 4)
        return std::nullopt;

    const double widthMeters = dimensions->first;
    const double heightMeters = dimensions->second;

    QVector2D widthVector(uniquePoints.at(1).x() - uniquePoints.at(0).x(),
                          uniquePoints.at(1).y() - uniquePoints.at(0).y());
    QVector2D heightVector(uniquePoints.at(2).x() - uniquePoints.at(1).x(),
                           uniquePoints.at(2).y() - uniquePoints.at(1).y());

    if (widthVector.lengthSquared() <= 0.0 || heightVector.lengthSquared() <= 0.0)
        return std::nullopt;

    const QVector2D widthDirection = widthVector.normalized();
    const QVector2D heightDirection = heightVector.normalized();

    double centerX = 0.0;
    double centerY = 0.0;
    for (int i = 0; i < 4; ++i) {
        centerX += uniquePoints.at(i).x();
        centerY += uniquePoints.at(i).y();
    }
    centerX /= 4.0;
    centerY /= 4.0;

    const double expandedHalfWidth = widthMeters / 2.0 + car_width;
    const double expandedHalfHeight = heightMeters / 2.0 + car_width;

    auto cornerAt = [&](double widthOffset, double heightOffset) {
        const double x = centerX + widthDirection.x() * widthOffset + heightDirection.x() * heightOffset;
        const double y = centerY + widthDirection.y() * widthOffset + heightDirection.y() * heightOffset;
        const Point webPoint(x, y, webMercator);
        return geometry_cast<Point>(GeometryEngine::project(webPoint, wgs84));
    };

    QList<Point> polygon;
    polygon << cornerAt(-expandedHalfWidth, -expandedHalfHeight)
            << cornerAt(expandedHalfWidth, -expandedHalfHeight)
            << cornerAt(expandedHalfWidth, expandedHalfHeight)
            << cornerAt(-expandedHalfWidth, expandedHalfHeight);

    return polygon;
}

void Test_map_widget::importImage()
{
    if (!m_imageOverlay)
        return;

    const bool hasImage = m_imageOverlay->hasImage();
    const bool imagePinned = hasImage && m_isImagePinned && m_imageOverlay->isPinned();

    if (imagePinned) {
        m_isImagePinned = false;
        m_imageOverlay->setPinnedMode(false);
        clearWorkAreaGraphic();
        m_workAreaVisible = false;
        m_cachedWorkArea.clear();
        statusBar()->showMessage(tr("Image unlocked. Drag to move, use the mouse wheel to zoom, and hold Shift while using the wheel to rotate."),
                                 8000);
        updateUiState();
        return;
    }

    const QString filePath = QFileDialog::getOpenFileName(this,
                                                         tr("Import image"),
                                                         QString(),
                                                         tr("Image Files (*.png *.jpg *.jpeg *.bmp *.gif *.tif *.tiff);;All Files (*)"));
    if (filePath.isEmpty())
        return;

    if (m_imageOverlay->loadImage(filePath)) {
        m_isImagePinned = false;
        m_pinnedImageMapPoints.clear();
        m_imageOverlay->setPinnedMode(false);
        m_hasCommittedGridChanges = false;
        m_originalImagePixmap = m_imageOverlay->currentPixmap();
        ++m_imageSessionCounter;
        m_currentImageSessionId = m_imageSessionCounter;
        m_gridWindowImageSessionId = 0;
        if (m_gridWindow) {
            m_gridWindow->resetState();
            m_gridWindow->hide();
        }
        const QFileInfo info(filePath);
        statusBar()->showMessage(tr("Loaded %1. Drag to move, use the mouse wheel to zoom, and hold Shift while using the wheel to rotate.")
                                     .arg(info.fileName()),
                                 8000);
        updateUiState();
    } else {
        statusBar()->showMessage(tr("Failed to load image."), 5000);
        updateUiState();
    }
}

void Test_map_widget::clearImportedImage()
{
    if (!m_imageOverlay)
        return;

    if (m_imageOverlay->hasImage()) {
        m_imageOverlay->clearImage();
        m_isImagePinned = false;
        m_pinnedImageMapPoints.clear();
        g_pinnedImageFootprint.clear();
        g_channelGrid.clear();
        m_hasCommittedGridChanges = false;
        m_originalImagePixmap = QPixmap();
        m_currentImageSessionId = 0;
        m_gridWindowImageSessionId = 0;
        clearWorkAreaGraphic();
        m_workAreaVisible = false;
        m_cachedWorkArea.clear();
        if (m_gridWindow) {
            m_gridWindow->resetState();
            m_gridWindow->hide();
        }
        statusBar()->showMessage(tr("Image removed."), 5000);
    }

    updateUiState();
}

void Test_map_widget::setImagePosition()
{
    if (!m_imageOverlay || !m_mapView || !m_graphicsOverlay)
        return;

    if (!m_imageOverlay->hasImage()) {
        statusBar()->showMessage(tr("Import an image before setting its position."), 5000);
        return;
    }

    if (!isCurrentImageAreaAcceptable()) {
        statusBar()->showMessage(tr("The image footprint is too large to pin."), 5000);
        return;
    }

    const auto mapPointsOptional = mapPointsForCurrentImageViewport();
    if (!mapPointsOptional) {
        statusBar()->showMessage(tr("Unable to determine the image footprint."), 5000);
        return;
    }

    const QList<Point> mapPoints = mapPointsOptional.value();

    m_pinnedImageMapPoints = mapPoints;
    m_isImagePinned = true;
    m_imageOverlay->setPinnedMode(true);
    clearWorkAreaGraphic();
    m_workAreaVisible = false;
    m_cachedWorkArea.clear();
    updatePinnedImagePosition();
    ++m_imageSessionCounter;
    m_currentImageSessionId = m_imageSessionCounter;
    m_gridWindowImageSessionId = 0;
    if (m_gridWindow) {
        m_gridWindow->resetState();
        m_gridWindow->hide();
    }
    statusBar()->showMessage(tr("Image pinned to the map. It will follow as you move and zoom."), 5000);
    updateUiState();
}

void Test_map_widget::updatePinnedImagePosition()
{
    if (!m_isImagePinned || !m_imageOverlay || !m_mapView)
        return;

    if (!m_imageOverlay->hasImage()) {
        m_isImagePinned = false;
        m_pinnedImageMapPoints.clear();
        updateUiState();
        return;
    }

    if (m_pinnedImageMapPoints.size() < 3)
        return;

    QPolygonF viewportPolygon;
    viewportPolygon.reserve(m_pinnedImageMapPoints.size());

    for (const Point &mapPoint : m_pinnedImageMapPoints) {
        const QPointF screenPoint = m_mapView->locationToScreen(mapPoint);
        viewportPolygon << screenPoint;
    }

    if (viewportPolygon.size() < 3)
        return;

    m_imageOverlay->applyViewportPolygon(viewportPolygon);

}

void Test_map_widget::openGridPreview()
{
    if (!m_imageOverlay || !m_imageOverlay->hasImage() || !m_isImagePinned)
        return;

    const auto dimensions = pinnedImageDimensionsMeters();
    if (!dimensions) {
        statusBar()->showMessage(tr("Unable to determine the pinned image dimensions."), 5000);
        return;
    }

    const QPixmap pixmap = m_imageOverlay->currentPixmap();
    if (pixmap.isNull()) {
        statusBar()->showMessage(tr("Unable to load the pinned image preview."), 5000);
        return;
    }

    if (!m_gridWindow) {
        m_gridWindow = new GridPreviewWindow(this);
        connect(m_gridWindow, &QObject::destroyed, this, [this]() {
            m_gridWindow = nullptr;
            m_gridWindowImageSessionId = 0;
        });
        connect(m_gridWindow, &GridPreviewWindow::effectCommitted, this,
                &Test_map_widget::applyCommittedGridEffect);
    }

    g_pinnedImageFootprint.clear();
    g_pinnedImageFootprint.reserve(m_pinnedImageMapPoints.size());
    for (const Point &point : m_pinnedImageMapPoints)
        g_pinnedImageFootprint << QPointF(point.x(), point.y());

    if (m_originalImagePixmap.isNull() && !pixmap.isNull() && !m_hasCommittedGridChanges)
        m_originalImagePixmap = pixmap;

    const bool needsInitialization = !m_gridWindow->hasSession() ||
                                     m_gridWindowImageSessionId != m_currentImageSessionId;

    if (needsInitialization) {
        const QPixmap basePixmap = m_originalImagePixmap.isNull() ? pixmap : m_originalImagePixmap;
        m_gridWindow->setImageWithGrid(basePixmap, dimensions->first, dimensions->second);
        m_gridWindowImageSessionId = m_currentImageSessionId;
    }

    m_gridWindow->show();
    m_gridWindow->raise();
    m_gridWindow->activateWindow();
}

void Test_map_widget::applyCommittedGridEffect(const QPixmap &pixmap, const QVector<QVector<int>> &seedChannels)
{
    if (!m_imageOverlay || pixmap.isNull())
        return;

    g_channelGrid = seedChannels;

    m_imageOverlay->setCurrentPixmap(pixmap);
    if (m_isImagePinned)
        updatePinnedImagePosition();

    m_hasCommittedGridChanges = true;

    if (statusBar())
        statusBar()->showMessage(tr("Grid effect applied to the pinned image."), 5000);

    updateUiState();
}

void Test_map_widget::updateUiState()
{
    const bool hasImage = m_imageOverlay && m_imageOverlay->hasImage();
    const bool hasPinnedImage = hasImage && m_isImagePinned && m_imageOverlay && m_imageOverlay->isPinned();

    if (!hasPinnedImage) {
        if (m_workAreaVisible)
            clearWorkAreaGraphic();
        m_workAreaVisible = false;
        m_cachedWorkArea.clear();
    }

    if (ui->showWorkAreaButton) {
        ui->showWorkAreaButton->setEnabled(hasPinnedImage);
        ui->showWorkAreaButton->setText(m_workAreaVisible ? tr("Hide Work Area") : tr("Show Work Area"));
    }

    if (ui->generatePathButton) {
        const bool canGeneratePath = hasPinnedImage && m_hasCommittedGridChanges && !g_channelGrid.isEmpty();
        ui->generatePathButton->setEnabled(canGeneratePath);
    }

    if (ui->importImageButton) {
        if (!hasImage) {
            ui->importImageButton->setText(tr("Load Image"));
            ui->importImageButton->setToolTip(tr("Select an image to display on top of the map"));
            ui->importImageButton->setEnabled(true);
        } else if (m_hasCommittedGridChanges) {
            ui->importImageButton->setText(tr("Load Image"));
            ui->importImageButton->setToolTip(tr("Close the current image before loading a new one."));
            ui->importImageButton->setEnabled(false);
        } else if (hasPinnedImage) {
            ui->importImageButton->setText(tr("Modify Position"));
            ui->importImageButton->setToolTip(tr("Return the pinned image to modify mode"));
            ui->importImageButton->setEnabled(true);
        } else {
            ui->importImageButton->setText(tr("Modify Position"));
            ui->importImageButton->setToolTip(tr("The image can already be dragged and rotated."));
            ui->importImageButton->setEnabled(false);
        }
    }

    if (ui->removeImageButton)
        ui->removeImageButton->setEnabled(hasImage);

    const bool areaAcceptable = hasImage && !hasPinnedImage && isCurrentImageAreaAcceptable();
    if (ui->setPositionButton)
        ui->setPositionButton->setEnabled(areaAcceptable);

    if (ui->openGridButton)
        ui->openGridButton->setEnabled(hasPinnedImage);

    updatePlacementInfoPanel(hasImage, hasPinnedImage);
    updateObstacleControls();
}

std::optional<QList<Point>> Test_map_widget::mapPointsForCurrentImageViewport() const
{
    if (!m_imageOverlay || !m_mapView || !m_imageOverlay->hasImage())
        return std::nullopt;

    const QPolygonF viewportPolygon = m_imageOverlay->currentImageViewportPolygon();
    if (viewportPolygon.size() < 3)
        return std::nullopt;

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

    if (mapPoints.size() > 1 && pointsApproximatelyEqual(mapPoints.first(), mapPoints.last()))
        mapPoints.removeLast();

    if (mapPoints.size() < 3)
        return std::nullopt;

    return mapPoints;
}

std::optional<double> Test_map_widget::currentImageAreaSquareMeters() const
{
    const auto mapPointsOptional = mapPointsForCurrentImageViewport();
    if (!mapPointsOptional || mapPointsOptional->size() < 3)
        return std::nullopt;

    const SpatialReference targetReference = SpatialReference::webMercator();
    PolygonBuilder builder(targetReference);

    for (const Point &point : mapPointsOptional.value()) {
        Point projectedPoint = point;
        if (projectedPoint.spatialReference().isEmpty() || projectedPoint.spatialReference() != targetReference)
            projectedPoint = geometry_cast<Point>(GeometryEngine::project(point, targetReference));
        builder.addPoint(projectedPoint);
    }

    const Geometry polygon = builder.toGeometry();
    const double area = std::abs(GeometryEngine::area(polygon));
    if (!std::isfinite(area) || area <= 0.0)
        return std::nullopt;

    return area;
}

bool Test_map_widget::isCurrentImageAreaAcceptable() const
{
    const auto areaOptional = currentImageAreaSquareMeters();
    return areaOptional && *areaOptional < max_image_area;
}

std::optional<std::pair<double, double>> Test_map_widget::currentImageDimensionsMeters() const
{
    const auto mapPointsOptional = mapPointsForCurrentImageViewport();
    if (!mapPointsOptional)
        return std::nullopt;

    return imageDimensionsMetersFromMapPoints(*mapPointsOptional);
}

std::optional<std::pair<double, double>> Test_map_widget::pinnedImageDimensionsMeters() const
{
    if (!m_isImagePinned || m_pinnedImageMapPoints.size() < 3)
        return std::nullopt;

    return imageDimensionsMetersFromMapPoints(m_pinnedImageMapPoints);
}

std::optional<std::pair<double, double>> Test_map_widget::imageDimensionsMetersFromMapPoints(const QList<Point> &mapPoints) const
{
    if (mapPoints.size() < 3)
        return std::nullopt;

    const SpatialReference targetReference = SpatialReference::webMercator();
    const auto projectPoint = [&targetReference](const Point &point) {
        if (point.spatialReference().isEmpty() || point.spatialReference() != targetReference)
            return geometry_cast<Point>(GeometryEngine::project(point, targetReference));
        return point;
    };

    const Point p0 = projectPoint(mapPoints.at(0));
    const Point p1 = projectPoint(mapPoints.at(1));
    const Point p2 = projectPoint(mapPoints.at(2));

    const double widthMeters = GeometryEngine::distance(p0, p1);
    const double heightMeters = GeometryEngine::distance(p1, p2);

    if (!std::isfinite(widthMeters) || !std::isfinite(heightMeters) || widthMeters <= 0.0 || heightMeters <= 0.0)
        return std::nullopt;

    return std::make_pair(widthMeters, heightMeters);
}

void Test_map_widget::updatePlacementInfoPanel(bool hasImage, bool hasPinnedImage)
{
    if (!ui || !ui->imagePlacementInfoGroup)
        return;

    const auto setPlacementInfoText = [this](const QString &widthText,
                                             const QString &heightText,
                                             const QString &widthGridText,
                                             const QString &heightGridText) {
        if (!ui->imagePlacementInfoValue)
            return;

        const QString formattedText = tr("Width: %1\nHeight: %2\nWidth grid #: %3\nHeight grid #: %4")
                                          .arg(widthText, heightText, widthGridText, heightGridText);
        ui->imagePlacementInfoValue->setText(formattedText);
    };

    const QString placeholder = QStringLiteral("---");
    const bool showPanel = hasImage && !hasPinnedImage;
    ui->imagePlacementInfoGroup->setVisible(showPanel);

    if (showPanel) {
        if (auto *layout = ui->imagePlacementInfoGroup->layout())
            layout->activate();
        ui->imagePlacementInfoGroup->adjustSize();
    } else {
        setPlacementInfoText(placeholder, placeholder, placeholder, placeholder);
        return;
    }

    const auto dimensionsOptional = currentImageDimensionsMeters();
    const QPixmap currentPixmap = m_imageOverlay ? m_imageOverlay->currentPixmap() : QPixmap();

    const QLocale locale;

    QString widthText = QStringLiteral("---");
    QString heightText = QStringLiteral("---");
    QString widthGridText = QStringLiteral("---");
    QString heightGridText = QStringLiteral("---");

    const auto areaOptional = currentImageAreaSquareMeters();
    if (areaOptional && *areaOptional >= max_image_area) {
        const QString invalidText = QStringLiteral("--");
        setPlacementInfoText(invalidText, invalidText, invalidText, invalidText);
        return;
    }

    if (dimensionsOptional) {
        const double widthMeters = dimensionsOptional->first;
        const double heightMeters = dimensionsOptional->second;

        widthText = tr("%1 m").arg(locale.toString(widthMeters, 'f', 2));
        heightText = tr("%1 m").arg(locale.toString(heightMeters, 'f', 2));

        if (!currentPixmap.isNull()) {
            const auto computeGridCount = [](double lengthMeters, int pixelCount) {
                if (lengthMeters <= 0.0 || pixelCount <= 0)
                    return 0;

                const double spacingPx = pixelCount * (grid_size / lengthMeters);
                if (!std::isfinite(spacingPx) || spacingPx < 1.0)
                    return 0;

                const int cellWidthPx = std::max(1, static_cast<int>(std::round(spacingPx)));
                if (cellWidthPx <= 0)
                    return 0;

                return pixelCount / cellWidthPx;
            };

            const int widthGrid = computeGridCount(widthMeters, currentPixmap.width());
            const int heightGrid = computeGridCount(heightMeters, currentPixmap.height());

            widthGridText = locale.toString(widthGrid);
            heightGridText = locale.toString(heightGrid);
        }
    }

    setPlacementInfoText(widthText, heightText, widthGridText, heightGridText);
}

void Test_map_widget::handleObstacleActionButton()
{
    if (m_isCapturingObstacle)
        finishObstacleCapture();
    else
        startObstacleCapture();
}

void Test_map_widget::startObstacleCapture()
{
    resetObstacleCreationState(true);
    m_isCapturingObstacle = true;

    updateObstacleControls();

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
    updateObstacleControls();

    if (statusBar())
        statusBar()->showMessage(tr("Obstacle saved."), 5000);
}

void Test_map_widget::cancelObstacleCapture()
{
    if (m_isCapturingObstacle) {
        resetObstacleCreationState(false);
        updateObstacleControls();

        if (statusBar())
            statusBar()->showMessage(tr("Obstacle creation canceled."), 5000);
        return;
    }

    if (obstaclesList.isEmpty())
        return;

    obstaclesList.removeLast();

    if (m_obstacleOverlay) {
        if (auto *graphicsModel = m_obstacleOverlay->graphics()) {
            const int lastIndex = graphicsModel->rowCount() - 1;
            if (lastIndex >= 0)
                graphicsModel->removeAt(lastIndex);
        }
    }

    updateObstacleControls();

    if (statusBar())
        statusBar()->showMessage(tr("Last obstacle deleted."), 5000);
}

void Test_map_widget::toggleWorkArea()
{
    if (!m_isImagePinned) {
        if (statusBar())
            statusBar()->showMessage(tr("Pin an image before showing the work area."), 5000);
        return;
    }

    if (!m_workAreaVisible) {
        if (!ensureWorkAreaGraphic()) {
            if (statusBar())
                statusBar()->showMessage(tr("Unable to determine the work area."), 5000);
            return;
        }
        m_workAreaVisible = true;
    } else {
        clearWorkAreaGraphic();
        m_workAreaVisible = false;
    }

    updateUiState();
}

void Test_map_widget::generatePathForCurrentImage()
{
    if (!m_isImagePinned || !m_imageOverlay || !m_imageOverlay->hasImage()) {
        if (statusBar())
            statusBar()->showMessage(tr("Pin an image before generating a path."), 5000);
        return;
    }

    if (g_channelGrid.isEmpty()) {
        if (statusBar())
            statusBar()->showMessage(tr("Allocate seeds before generating a path."), 5000);
        return;
    }

    std::optional<QList<Point>> workAreaPoints;
    if (m_workAreaVisible && !m_cachedWorkArea.isEmpty())
        workAreaPoints = m_cachedWorkArea;
    else
        workAreaPoints = workAreaRectangle();

    if (!workAreaPoints) {
        if (statusBar())
            statusBar()->showMessage(tr("Unable to determine the work area."), 5000);
        return;
    }

    QList<QList<Point>> obstaclePolygons;
    obstaclePolygons.reserve(obstaclesList.size());
    for (const obstacles &obstacle : obstaclesList)
        obstaclePolygons.append(obstacle.vertices);

    generate_path(*workAreaPoints, g_channelGrid);

    if (statusBar())
        statusBar()->showMessage(tr("Path generation requested."), 5000);
}

void Test_map_widget::addObstaclePoint(const QPoint &screenPoint)
{
    if (!m_isCapturingObstacle || !m_mapView)
        return;

    const Point mapPoint = m_mapView->screenToLocation(screenPoint.x(),screenPoint.y());
    m_currentObstaclePoints.append(mapPoint);

    rebuildObstaclePreview();

    updateObstacleControls();
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

    updateObstacleControls();
}

void Test_map_widget::updateObstacleControls()
{
    if (!ui)
        return;

    if (ui->addObstacleButton) {
        if (m_isCapturingObstacle) {
            const bool canFinish = m_currentObstaclePoints.size() >= 3;
            ui->addObstacleButton->setText(tr("Finish"));
            ui->addObstacleButton->setEnabled(canFinish);
        } else {
            ui->addObstacleButton->setText(tr("Add Obstacle"));
            ui->addObstacleButton->setEnabled(true);
        }
    }

    if (ui->cancelObstacleButton) {
        if (m_isCapturingObstacle) {
            ui->cancelObstacleButton->setText(tr("Cancel"));
            ui->cancelObstacleButton->setEnabled(true);
        } else {
            ui->cancelObstacleButton->setText(tr("Delete"));
            ui->cancelObstacleButton->setEnabled(!obstaclesList.isEmpty());
        }
    }
}
