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
#include <QList>

// C++ API headers
#include "Graphic.h"
#include "GraphicListModel.h"
#include "GraphicsOverlay.h"
#include "GraphicsOverlayListModel.h"
#include "Geometry.h"
#include "Map.h"
#include "MapGraphicsView.h"
#include "MapTypes.h"
#include "Point.h"
#include "PolylineBuilder.h"
#include "SimpleLineSymbol.h"
#include "SimpleLineSymbolStyle.h"
#include "SpatialReference.h"

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

    connect(ui->goToCoordinateButton, &QPushButton::clicked, this, &Test_map_widget::goToCoordinates);
    connect(ui->drawLineButton, &QPushButton::clicked, this, &Test_map_widget::drawLineBetweenCoordinates);
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

void Test_map_widget::drawLineBetweenCoordinates()
{
    if (!m_mapView)
        return;

    const QString coordinateText = ui->lineCoordinateInput->text().trimmed();
    if (coordinateText.isEmpty()) {
        statusBar()->showMessage(tr("Enter two coordinate pairs as lat, lon; lat, lon."), 5000);
        return;
    }

    QString normalized = coordinateText;
    normalized.replace(QStringLiteral("->"), QLatin1String(" "));
    const QList<QChar> separators{QLatin1Char(','), QLatin1Char(';'), QLatin1Char('\n')};
    for (const QChar &separator : separators)
        normalized.replace(separator, QLatin1Char(' '));

    const QStringList parts = normalized.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    if (parts.size() != 4) {
        statusBar()->showMessage(tr("Enter two coordinate pairs as lat, lon; lat, lon."), 5000);
        return;
    }

    bool okValues[4] = {false, false, false, false};
    double values[4] = {0.0, 0.0, 0.0, 0.0};
    for (int i = 0; i < 4; ++i) {
        values[i] = parts.at(i).toDouble(&okValues[i]);
        if (!okValues[i]) {
            statusBar()->showMessage(tr("Coordinates must be valid latitude and longitude."), 5000);
            return;
        }
    }

    const double startLatitude = values[0];
    const double startLongitude = values[1];
    const double endLatitude = values[2];
    const double endLongitude = values[3];

    auto isLatitudeValid = [](double latitude) { return latitude >= -90.0 && latitude <= 90.0; };
    auto isLongitudeValid = [](double longitude) { return longitude >= -180.0 && longitude <= 180.0; };

    if (!isLatitudeValid(startLatitude) || !isLatitudeValid(endLatitude) || !isLongitudeValid(startLongitude) || !isLongitudeValid(endLongitude)) {
        statusBar()->showMessage(tr("Coordinates must be valid latitude and longitude."), 5000);
        return;
    }

    const Point startPoint(startLongitude, startLatitude, SpatialReference::wgs84());
    const Point endPoint(endLongitude, endLatitude, SpatialReference::wgs84());

    drawLineBetweenPoints(startPoint, endPoint);

    statusBar()->showMessage(
        tr("Drew line from %1, %2 to %3, %4")
            .arg(startLatitude, 0, 'f', 4)
            .arg(startLongitude, 0, 'f', 4)
            .arg(endLatitude, 0, 'f', 4)
            .arg(endLongitude, 0, 'f', 4),
        5000);
}

void Test_map_widget::drawLineBetweenPoints(const Point &start, const Point &end)
{
    if (!m_mapView)
        return;

    if (!m_graphicsOverlay) {
        m_graphicsOverlay = new GraphicsOverlay(this);
        m_mapView->graphicsOverlays()->append(m_graphicsOverlay);
    }

    PolylineBuilder builder(SpatialReference::wgs84());
    builder.addPoint(start);
    builder.addPoint(end);
    const Geometry lineGeometry = builder.toGeometry();

    auto *lineSymbol = new SimpleLineSymbol(SimpleLineSymbolStyle::Solid, QColor(0, 122, 204), 3.0f, m_graphicsOverlay);
    auto *lineGraphic = new Graphic(lineGeometry, lineSymbol, m_graphicsOverlay);

    m_graphicsOverlay->graphics()->append(lineGraphic);

    m_mapView->setViewpointGeometryAsync(lineGeometry, 50.0);
}
