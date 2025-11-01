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

double dis(QPair<double,double> a,QPair<double,double> b){
    return pow(a.first-b.first,2)+pow(a.second-b.second,2);
}



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

    const int totalRows = channelGrid.size();
    const int totalColumns=channelGrid.begin()->size();

    //get starting and ending positions of each row
    QVector<QPair<double, double>> pathCoordinates;
    QVector<std::array<int, 3>> p;
    bool ch;
    for (int i=0;i<totalRows;i++) {
        ch=0;
        for (int j=0;j<channelGrid[i].size();j++)
            if(channelGrid[i][j]>0){
                ch=1,p.append({i,j,j});
                break;
            }
        if(ch==0) break;
        for (int j=channelGrid[i].size()-1;j>=0;j--)
            if(channelGrid[i][j]>0){
                p.back()[2]=j;
            }
    }

    //append current coordinate
    pathCoordinates.append(QPair<double, double>(currentGpsPoint.x(),currentGpsPoint.y()));
    if(p.size()==0)return;

    //decide starting row
    double tt[2][2];
    for (int i=0;i<=1;i++)
        tt[0][i]=dis(pathCoordinates[0],gridCellGpsCoordinate(p.front()[0],p.front()[i],workAreaPolygon,totalRows,totalColumns));
    for (int i=0;i<=1;i++)
        tt[1][i]=dis(pathCoordinates[0],gridCellGpsCoordinate(p.back()[0],p.back()[i],workAreaPolygon,totalRows,totalColumns));
    if(std::min(tt[0][0],tt[0][1])>std::min(tt[1][0],tt[1][1]))
        std::reverse(p.begin(),p.end());

    //generate path point
    QPair<double,double> p1,p2;
    for (int i=0;i<p.size();i++){
        p1=gridCellGpsCoordinate(p[i][0],p[i][1],workAreaPolygon,totalRows,totalColumns);
        p2=gridCellGpsCoordinate(p[i][0],p[i][2],workAreaPolygon,totalRows,totalColumns);
        if(dis(pathCoordinates.back(),p1)>dis(pathCoordinates.back(),p2))
            std::swap(p1,p2);
        pathCoordinates.append(p1),pathCoordinates.append(p2);
    }

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
}
QPair<double,double> gridCellGpsCoordinate(int row,
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
    return QPair<double,double> (webPoint.x()/1e5,webPoint.y()/1e5);
}
