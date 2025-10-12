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
#include <QRegularExpression>
#include <QStatusBar>
#include <QStringList>
#include <QEvent>

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

    connect(ui->goToCoordinateButton, &QPushButton::clicked, this, &Test_map_widget::goToCoordinates);
    connect(ui->importImageButton, &QPushButton::clicked, this, &Test_map_widget::importImage);
    connect(ui->removeImageButton, &QPushButton::clicked, this, &Test_map_widget::clearImportedImage);
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
