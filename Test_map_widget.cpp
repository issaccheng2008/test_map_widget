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
#include "SeedSelectionWindow.h"
#include "generate_path.h"

// Qt headers
#include <QColor>
#include <QImageReader>
#include <QPoint>
#include <QFileDialog>
#include <QFileInfo>
#include <QFuture>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QStatusBar>
#include <QStringList>
#include <QEvent>
#include <QtMath>

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
#include "Polygon.h"
#include "SimpleFillSymbol.h"
#include "SimpleLineSymbol.h"
#include "SpatialReference.h"
#include "SimpleMarkerSymbol.h"
#include "SymbolTypes.h"
#include "PolylineBuilder.h"
#include "PartCollection.h"
#include "Part.h"
#include "GraphicListModel.h"
#include "GraphicsOverlayListModel.h"

#include "ui_Test_map_widget.h"

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

    connect(m_imageOverlay, &OverlayImageWidget::imageLoaded, this, &Test_map_widget::onOverlayImageLoaded);
    connect(m_imageOverlay, &OverlayImageWidget::imageCleared, this, &Test_map_widget::onOverlayImageCleared);

    // Set map to map view
    m_mapView->setMap(m_map);

    // Prepare a graphics overlay for displaying dynamic shapes
    m_graphicsOverlay = new GraphicsOverlay(this);
    m_mapView->graphicsOverlays()->append(m_graphicsOverlay);
    m_workAreaOverlay = new GraphicsOverlay(this);
    m_mapView->graphicsOverlays()->append(m_workAreaOverlay);

    m_seedSelectionWindow = new SeedSelectionWindow(this);
    connect(m_seedSelectionWindow, &SeedSelectionWindow::seedsChanged, this, &Test_map_widget::onSeedsChanged);

    connect(ui->goToCoordinateButton, &QPushButton::clicked, this, &Test_map_widget::goToCoordinates);
    connect(ui->importImageButton, &QPushButton::clicked, this, &Test_map_widget::importImage);
    connect(ui->removeImageButton, &QPushButton::clicked, this, &Test_map_widget::clearImportedImage);
    connect(ui->showWorkAreaButton, &QPushButton::clicked, this, &Test_map_widget::toggleWorkArea);
    connect(ui->allocateSeedsButton, &QPushButton::clicked, this, &Test_map_widget::openSeedSelectionWindow);
    connect(ui->generatePathButton, &QPushButton::clicked, this, &Test_map_widget::generatePath);
    // Connect the exit button created in the UI to close the window
    connect(ui->exitButton, &QPushButton::clicked, this, &QWidget::close);

    ui->showWorkAreaButton->setEnabled(false);
    ui->allocateSeedsButton->setEnabled(false);
    ui->generatePathButton->setEnabled(false);

    updateImageDependentUi();
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

void Test_map_widget::toggleWorkArea()
{
    if (!m_imageOverlay || !m_imageOverlay->hasImage() || !m_workAreaOverlay)
        return;

    if (m_isWorkAreaVisible) {
        hideWorkAreaGraphic();
        return;
    }

    const Geometry workAreaGeometry = buildWorkAreaGeometry();
    if (workAreaGeometry.isEmpty()) {
        statusBar()->showMessage(tr("Unable to determine the work area."), 5000);
        return;
    }

    if (auto *graphics = m_workAreaOverlay->graphics())
        graphics->clear();

    const QColor outlineColor(0, 255, 0, 200);
    auto *outlineSymbol = new SimpleLineSymbol(SimpleLineSymbolStyle::Solid, outlineColor, 2.0f, this);
    auto *fillSymbol = new SimpleFillSymbol(SimpleFillSymbolStyle::Solid, QColor(0, 255, 0, 40), outlineSymbol, this);

    m_workAreaGraphic = new Graphic(workAreaGeometry, fillSymbol, m_workAreaOverlay);
    m_workAreaOverlay->graphics()->append(m_workAreaGraphic);
    m_isWorkAreaVisible = true;
    ui->showWorkAreaButton->setText(tr("Hide Work Area"));
}

void Test_map_widget::openSeedSelectionWindow()
{
    if (!m_seedSelectionWindow)
        return;

    m_seedSelectionWindow->show();
    m_seedSelectionWindow->raise();
    m_seedSelectionWindow->activateWindow();
}

void Test_map_widget::generatePath()
{
    if (!m_seedSelectionWindow || !m_seedSelectionWindow->hasAllocatedSeeds())
        return;

    const Geometry bufferedArea = buildWorkAreaGeometry();
    if (bufferedArea.isEmpty()) {
        statusBar()->showMessage(tr("No work area is available."), 5000);
        return;
    }

    QList<Point> rectangleVertices;
    const Polygon polygon = geometry_cast<Polygon>(bufferedArea);
    if (!polygon.isEmpty() && polygon.parts().size() > 0) {
        const Part &part = polygon.parts().at(0);
        for (int i = 0; i < part.pointCount(); ++i)
            rectangleVertices.append(part.point(i));
    }

    if (rectangleVertices.isEmpty()) {
        statusBar()->showMessage(tr("Unable to extract work area coordinates."), 5000);
        return;
    }

    generate_path(rectangleVertices, m_obstacles, m_seedSelectionWindow->channelGrid());
    statusBar()->showMessage(tr("Generating path..."), 4000);
}

void Test_map_widget::onSeedsChanged()
{
    updateGeneratePathButtonState();
}

void Test_map_widget::onOverlayImageLoaded()
{
    hideWorkAreaGraphic();
    updateImageDependentUi();
}

void Test_map_widget::onOverlayImageCleared()
{
    hideWorkAreaGraphic();
    updateImageDependentUi();
}

void Test_map_widget::updateImageDependentUi()
{
    const bool hasImage = m_imageOverlay && m_imageOverlay->hasImage();

    if (ui->showWorkAreaButton) {
        ui->showWorkAreaButton->setEnabled(hasImage);
        if (!hasImage)
            ui->showWorkAreaButton->setText(tr("Show Work Area"));
    }

    if (!hasImage)
        hideWorkAreaGraphic();

    if (ui->allocateSeedsButton)
        ui->allocateSeedsButton->setEnabled(hasImage);

    updateGeneratePathButtonState();
}

void Test_map_widget::updateGeneratePathButtonState()
{
    const bool hasImage = m_imageOverlay && m_imageOverlay->hasImage();
    const bool hasSeeds = m_seedSelectionWindow && m_seedSelectionWindow->hasAllocatedSeeds();

    if (ui->generatePathButton)
        ui->generatePathButton->setEnabled(hasImage && hasSeeds);
}

void Test_map_widget::hideWorkAreaGraphic()
{
    if (m_workAreaOverlay && m_workAreaOverlay->graphics())
        m_workAreaOverlay->graphics()->clear();

    m_workAreaGraphic = nullptr;
    if (ui->showWorkAreaButton)
        ui->showWorkAreaButton->setText(tr("Show Work Area"));
    m_isWorkAreaVisible = false;
}

QList<Point> Test_map_widget::workAreaCoordinates() const
{
    QList<Point> coordinates;
    if (!m_mapView || !m_imageOverlay || !m_imageOverlay->hasImage())
        return coordinates;

    const QPolygonF polygon = m_imageOverlay->imagePolygonInView();
    if (polygon.size() < 4)
        return coordinates;

    for (const QPointF &point : polygon) {
        const QPoint screenPoint = QPoint(qRound(point.x()), qRound(point.y()));
        const Point location = m_mapView->screenToLocation(screenPoint);
        if (!location.isEmpty())
            coordinates.append(location);
    }
    return coordinates;
}

Geometry Test_map_widget::buildWorkAreaGeometry() const
{
    const QList<Point> corners = workAreaCoordinates();
    if (corners.size() < 3)
        return Geometry();

    PolygonBuilder builder(SpatialReference::wgs84());
    for (const Point &corner : corners)
        builder.addPoint(corner);
    if (!corners.isEmpty())
        builder.addPoint(corners.first());

    const Geometry polygon = builder.toGeometry();
    if (polygon.isEmpty())
        return polygon;

    const Geometry projected = GeometryEngine::project(polygon, SpatialReference::webMercator());
    const Geometry buffered = GeometryEngine::buffer(projected, car_width);
    return GeometryEngine::project(buffered, SpatialReference::wgs84());
}

bool Test_map_widget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_mapView && event->type() == QEvent::Resize) {
        if (m_imageOverlay)
            m_imageOverlay->setGeometry(m_mapView->rect());
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

    QImageReader reader(filePath);
    const QSize imageSize = reader.size();
    if (imageSize.isValid()) {
        const double area = static_cast<double>(imageSize.width()) * imageSize.height();
        if (area > max_image_area) {
            statusBar()->showMessage(tr("Image exceeds the maximum supported area of %1 pixels.").arg(max_image_area, 0, 'f', 0), 6000);
            return;
        }
    }

    if (m_imageOverlay->loadImage(filePath)) {
        const QFileInfo info(filePath);
        statusBar()->showMessage(tr("Loaded %1. Drag to move, use the mouse wheel to zoom, and hold Shift while using the wheel to rotate.")
                                     .arg(info.fileName()),
                                 8000);
        hideWorkAreaGraphic();
        updateImageDependentUi();
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
        hideWorkAreaGraphic();
        updateImageDependentUi();
        statusBar()->showMessage(tr("Image removed."), 5000);
    }
}
