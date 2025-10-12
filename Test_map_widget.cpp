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

// Qt headers
#include <QColor>
#include <QFuture>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QStatusBar>
#include <QStringList>

// C++ API headers
#include "Graphic.h"
#include "GraphicsOverlay.h"
#include "Map.h"
#include "MapGraphicsView.h"
#include "MapTypes.h"
#include "Point.h"
#include "PolylineBuilder.h"
#include "SimpleLineSymbol.h"
#include "SpatialReference.h"
#include "GeometryEngine.h"
#include "Geometry.h"



#include "Graphic.h"
#include "GraphicListModel.h"
#include "GraphicsOverlay.h"
#include "GraphicsOverlayListModel.h"
#include "PolylineBuilder.h"
#include "PolygonBuilder.h"
#include "SimpleFillSymbol.h"
#include "SimpleLineSymbol.h"
#include "SimpleMarkerSymbol.h"
#include "SymbolTypes.h"

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

    // Set map to map view
    m_mapView->setMap(m_map);

    // Prepare a graphics overlay for displaying dynamic shapes
    m_graphicsOverlay = new GraphicsOverlay(this);
    m_mapView->graphicsOverlays()->append(m_graphicsOverlay);

    connect(ui->goToCoordinateButton, &QPushButton::clicked, this, &Test_map_widget::goToCoordinates);
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
    SpatialReference spatialRef = start.spatialReference().isEmpty() ? start.spatialReference() : end.spatialReference();
    if (!spatialRef.isEmpty())
        spatialRef = SpatialReference::wgs84();

    Point startPoint = start;
    if (startPoint.spatialReference() != spatialRef)
        startPoint = geometry_cast<Point>(GeometryEngine::project(startPoint, spatialRef));

    Point endPoint = end;
    if (endPoint.spatialReference() != spatialRef)
        endPoint = geometry_cast<Point>(GeometryEngine::project(endPoint, spatialRef));

    PolylineBuilder builder(spatialRef);
    builder.addPoint(startPoint);
    builder.addPoint(endPoint);

    const QColor lineColor(0, 0, 255, 127);
    auto *lineSymbol = new SimpleLineSymbol(SimpleLineSymbolStyle::Solid, lineColor, 1.5f, this);

    Graphic *lineGraphic = new Graphic(builder.toGeometry(), lineSymbol, this);
    m_graphicsOverlay->graphics()->append(lineGraphic);
}
