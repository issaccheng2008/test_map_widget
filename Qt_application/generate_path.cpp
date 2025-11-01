#include "generate_path.h"

#include <QDebug>

#include "Geometry.h"
#include "GeometryEngine.h"
#include "Point.h"
#include "SpatialReference.h"

#include <algorithm>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QPair>
#include <QLatin1Char>
#include <QStringList>
#include <QUrl>
#include <QVector>
#include <QVector2D>

using namespace Esri::ArcGISRuntime;

namespace {

Point projectToSpatialReference(const Point &point, const SpatialReference &targetReference)
{
    if (point.isEmpty())
        return {};

    if (!point.spatialReference().isEmpty() && point.spatialReference() == targetReference)
        return point;

    return geometry_cast<Point>(GeometryEngine::project(point, targetReference));
}

bool polygonHasRequiredCorners(const QList<Point> &polygon)
{
    return polygon.size() >= 4 && std::all_of(polygon.cbegin(), polygon.cbegin() + 4, [](const Point &p) {
               return !p.isEmpty();
           });
}

} // namespace

const double car_width = 0.6;
const double car_length = 1.5;
const double grid_size = 0.2;
const double max_image_area = 10000.0;
const int channel_number = 5;

void generate_path(const QList<Esri::ArcGISRuntime::Point> &workAreaPolygon,
                   const QVector<QVector<int>> &channelGrid,
                   const Esri::ArcGISRuntime::Point &currentGpsPoint)
{
    qDebug() << "generate_path called";
    if (!currentGpsPoint.isEmpty())
        qDebug() << "Current GPS location:" << currentGpsPoint.y() << currentGpsPoint.x();

    QVector<QPair<double, double>> pathCoordinates;
    pathCoordinates.reserve(4);
    pathCoordinates.append(QPair<double, double>(0.0, 0.0));
    pathCoordinates.append(QPair<double, double>(0.0, 10.0));
    pathCoordinates.append(QPair<double, double>(0.0, 20.0));
    pathCoordinates.append(QPair<double, double>(10.0, 20.0));

    QStringList trackPointLines;
    trackPointLines.reserve(pathCoordinates.size());
    for (const QPair<double, double> &coordinate : pathCoordinates) {
        const double latitude = coordinate.first;
        const double longitude = coordinate.second;
        const QString trackPointLine =
            QString("      <trkpt lat=\"%1\" lon=\"%2\" />").arg(latitude, 0, 'f', 6).arg(longitude, 0, 'f', 6);
        trackPointLines.append(trackPointLine);
    }

    const QString gpxHeader = QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                                              "<gpx version=\"1.1\" creator=\"TestMapWidget\">\n"
                                              "  <trk>\n"
                                              "    <name>Generated Path</name>\n"
                                              "    <trkseg>\n");
    const QString gpxFooter = QStringLiteral("    </trkseg>\n"
                                              "  </trk>\n"
                                              "</gpx>\n");

    QString gpxDocument = gpxHeader;
    if (!trackPointLines.isEmpty()) {
        gpxDocument += trackPointLines.join(QLatin1Char('\n'));
        gpxDocument += QLatin1Char('\n');
    }
    gpxDocument += gpxFooter;

    QJsonObject payloadObject;
    payloadObject.insert(QStringLiteral("type"), QStringLiteral("file"));
    payloadObject.insert(QStringLiteral("content"), gpxDocument);

    const QJsonDocument payloadDocument(payloadObject);
    const QByteArray jsonPayload = payloadDocument.toJson(QJsonDocument::Compact);

    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl(QStringLiteral("http://192.168.43.95/file")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QEventLoop loop;
    QObject::connect(&manager, &QNetworkAccessManager::finished, &loop, &QEventLoop::quit);

    QNetworkReply *reply = manager.post(request, jsonPayload);
    if (!reply) {
        qWarning() << "Failed to create network reply for GPX upload";
    } else {
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "Failed to upload GPX to ESP32:" << reply->errorString();
        } else {
            qDebug() << "Successfully uploaded GPX path to ESP32";
        }

        reply->deleteLater();
    }

    const int totalRows = channelGrid.size();
    int totalColumns = 0;
    for (const QVector<int> &row : channelGrid) {
        if (!row.isEmpty()) {
            totalColumns = row.size();
            break;
        }
    }

    // for (int i=0;i<totalRows;i++){

    // }
    if (polygonHasRequiredCorners(workAreaPolygon) && totalRows > 0 && totalColumns > 0) {
        const Point example = gridCellGpsCoordinate(0, 10, workAreaPolygon, totalRows, totalColumns);
        if (!example.isEmpty())
            qDebug() << "Example grid cell (0,0) center:" << example.x() << example.y();
    }
}
Point gridCellGpsCoordinate(int row,
                            int column,
                            const QList<Point> &workAreaPolygon,
                            int totalRows,
                            int totalColumns)
{
    if (!polygonHasRequiredCorners(workAreaPolygon) || totalRows <= 0 || totalColumns <= 0)
        return {};

    if (row < 0 || column < 0 || row >= totalRows || column >= totalColumns)
        return {};

    const SpatialReference webMercator = SpatialReference::webMercator();
    const SpatialReference wgs84 = SpatialReference::wgs84();

    const Point topLeft = projectToSpatialReference(workAreaPolygon.at(0), webMercator);
    const Point topRight = projectToSpatialReference(workAreaPolygon.at(1), webMercator);
    const Point bottomRight = projectToSpatialReference(workAreaPolygon.at(2), webMercator);
    const Point bottomLeft = projectToSpatialReference(workAreaPolygon.at(3), webMercator);

    if (topLeft.isEmpty() || topRight.isEmpty() || bottomRight.isEmpty() || bottomLeft.isEmpty())
        return {};

    const double columnFraction = (totalColumns == 1)
                                      ? 0.5
                                      : (static_cast<double>(column) + 0.5) / static_cast<double>(totalColumns);
    const double rowFraction = (totalRows == 1)
                                   ? 0.5
                                   : (static_cast<double>(row) + 0.5) / static_cast<double>(totalRows);
    const double topX = topLeft.x() + (topRight.x() - topLeft.x()) * columnFraction;
    const double topY = topLeft.y() + (topRight.y() - topLeft.y()) * columnFraction;

    const double bottomX = bottomLeft.x() + (bottomRight.x() - bottomLeft.x()) * columnFraction;
    const double bottomY = bottomLeft.y() + (bottomRight.y() - bottomLeft.y()) * columnFraction;

    const double interpolatedX = topX + (bottomX - topX) * rowFraction;
    const double interpolatedY = topY + (bottomY - topY) * rowFraction;

    const Point webPoint(interpolatedX, interpolatedY, webMercator);
    if (webPoint.isEmpty())
        return {};

    return geometry_cast<Point>(GeometryEngine::project(webPoint, wgs84));
}
