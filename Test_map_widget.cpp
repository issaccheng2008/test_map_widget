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
#include <QRegularExpressionMatch>
#include <QStatusBar>
#include <QStringList>
#include <QVector>

// C++ API headers
#include "Graphic.h"
#include "GraphicListModel.h"
#include "GraphicsOverlay.h"
#include "GraphicsOverlayListModel.h"
#include "Map.h"
#include "MapGraphicsView.h"
#include "MapTypes.h"
#include "Point.h"
#include "Polyline.h"
#include "PolylineBuilder.h"
#include "SpatialReference.h"
#include "SimpleLineSymbol.h"
#include "Envelope.h"

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

    m_graphicsOverlay = new GraphicsOverlay(this);
    m_mapView->graphicsOverlays()->append(m_graphicsOverlay);

    connect(ui->goToCoordinateButton, &QPushButton::clicked, this, &Test_map_widget::goToCoordinates);
    connect(ui->drawLineButton, &QPushButton::clicked, this, &Test_map_widget::drawLineFromInput);
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
}

void Test_map_widget::drawLineFromInput()
{
    if (!m_mapView)
        return;

    const QString coordinateText = ui->lineCoordinatesInput->text().trimmed();
    if (coordinateText.isEmpty()) {
        statusBar()->showMessage(tr("Enter two coordinates as lat1, lon1; lat2, lon2."), 5000);
        return;
    }

    QRegularExpression numberRegex(QStringLiteral("[-+]?\\d+(?:\\.\\d+)?"));
    QRegularExpressionMatchIterator it = numberRegex.globalMatch(coordinateText);
    QVector<double> values;
    values.reserve(4);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        bool ok = false;
        const double value = match.captured(0).toDouble(&ok);
        if (ok)
            values.append(value);
    }

    if (values.size() != 4) {
        statusBar()->showMessage(tr("Enter two coordinates as lat1, lon1; lat2, lon2."), 5000);
        return;
    }

    const double lat1 = values.at(0);
    const double lon1 = values.at(1);
    const double lat2 = values.at(2);
    const double lon2 = values.at(3);

    auto validLat = [](double latitude) { return latitude >= -90.0 && latitude <= 90.0; };
    auto validLon = [](double longitude) { return longitude >= -180.0 && longitude <= 180.0; };

    if (!validLat(lat1) || !validLat(lat2) || !validLon(lon1) || !validLon(lon2)) {
        statusBar()->showMessage(tr("Coordinates must be valid latitude and longitude."), 5000);
        return;
    }

    const Point startPoint(lon1, lat1, SpatialReference::wgs84());
    const Point endPoint(lon2, lat2, SpatialReference::wgs84());

    drawLineBetweenPoints(startPoint, endPoint);

    statusBar()->showMessage(tr("Drew line between (%1, %2) and (%3, %4)")
                                 .arg(lat1, 0, 'f', 4)
                                 .arg(lon1, 0, 'f', 4)
                                 .arg(lat2, 0, 'f', 4)
                                 .arg(lon2, 0, 'f', 4),
                             5000);
}

void Test_map_widget::drawLineBetweenPoints(const Point &start, const Point &end)
{
    if (!m_graphicsOverlay)
        return;

    PolylineBuilder builder(start.spatialReference());
    builder.addPoint(start);
    builder.addPoint(end);
    const Polyline polyline = builder.toGeometry();

    if (!m_routeGraphic) {
        auto *symbol = new SimpleLineSymbol(SimpleLineSymbolStyle::Solid, QColor(255, 0, 0), 3.0f, this);
        m_routeGraphic = new Graphic(polyline, symbol, this);
        m_graphicsOverlay->graphics()->append(m_routeGraphic);
    } else {
        m_routeGraphic->setGeometry(polyline);
    }

    m_mapView->setViewpointGeometryAsync(polyline.extent(), 50.0);
}
