#include "generate_path.h"

#include <QDebug>

#include "Geometry.h"
#include "GeometryEngine.h"
#include "Point.h"
#include "SpatialReference.h"

#include <algorithm>
#include <limits>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QLatin1Char>
#include <QStringList>
#include <QUrl>
#include <QVector>

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

double dis(const Point &a, const Point &b)
{
    if (a.isEmpty() || b.isEmpty())
        return std::numeric_limits<double>::infinity();

    const double dx = a.x() - b.x();
    const double dy = a.y() - b.y();
    return dx * dx + dy * dy;
}



const double car_width = 0.6;
const double car_length = 1.5;
const double grid_size = 0.2;
const double max_image_area = 10000.0;
const int channel_number = 5;

QVector<Point> generate_path(const QList<Esri::ArcGISRuntime::Point> &pinnedImageCorners,
                             const QVector<QVector<int>> &channelGrid,
                             const Esri::ArcGISRuntime::Point &currentGpsPoint)
{
    qDebug() << "generate_path called";
    if (!currentGpsPoint.isEmpty())
        qDebug() << "Current GPS location:" << currentGpsPoint.y() << currentGpsPoint.x();

    const int totalRows = channelGrid.size();
    const int totalColumns=channelGrid.begin()->size();

    //get starting and ending positions of each row
    QVector<Point> pathCoordinates;
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
                break;
            }
    }

    //append current coordinate
    const SpatialReference wgs84 = SpatialReference::wgs84();
    const auto ensureWgs84 = [&wgs84](const Point &point) {
        if (point.isEmpty())
            return Point();
        if (point.spatialReference().isEmpty() || point.spatialReference() == wgs84)
            return Point(point.x(), point.y(), wgs84);
        return geometry_cast<Point>(GeometryEngine::project(point, wgs84));
    };

    const Point currentPoint = ensureWgs84(currentGpsPoint);
    if (currentPoint.isEmpty())
        return {};

    pathCoordinates.append(currentPoint);
    if(p.size()==0)return pathCoordinates;

    //decide starting row
    double tt[2][2];
    for (int i=0;i<=1;i++)
        tt[0][i]=dis(pathCoordinates[0],gridCellGpsCoordinate(p.front()[0],p.front()[i],pinnedImageCorners,totalRows,totalColumns));
    for (int i=0;i<=1;i++)
        tt[1][i]=dis(pathCoordinates[0],gridCellGpsCoordinate(p.back()[0],p.back()[i],pinnedImageCorners,totalRows,totalColumns));

    int r=-1,ad=6;
    if(std::min(tt[0][0],tt[0][1])>std::min(tt[1][0],tt[1][1]))
        std::reverse(p.begin(),p.end()),r=totalColumns,ad=-6;

    //generate path point
    Point p1,p2;

    for (int i=0,mini,maxi;i<p.size();i++){
        r=p[i][0]+ad,mini=totalColumns-1,maxi=0;
        for (;i<p.size()&&(ad>0?p[i][0]<=r:p[i][0]>=r);i++)
            mini=std::min(mini,p[i][1]),maxi=std::max(maxi,p[i][2]);
        p1=gridCellGpsCoordinate(r-ad/2,mini,pinnedImageCorners,totalRows,totalColumns);
        p2=gridCellGpsCoordinate(r-ad/2,maxi,pinnedImageCorners,totalRows,totalColumns);

        if(dis(pathCoordinates.back(),p1)>dis(pathCoordinates.back(),p2))
            std::swap(p1,p2);
        pathCoordinates.append(p1),pathCoordinates.append(p2);
    }

    QStringList trackPointLines;
    trackPointLines.reserve(pathCoordinates.size());
    for (const Point &coordinate : pathCoordinates) {
        if (coordinate.isEmpty())
            continue;
        const double latitude = coordinate.y();
        const double longitude = coordinate.x();
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
    return pathCoordinates;
}
Point gridCellGpsCoordinate(int row,
                            int column,
                            const QList<Point> &pinnedImageCorners,
                            int totalRows,
                            int totalColumns)
{
    if (!polygonHasRequiredCorners(pinnedImageCorners) || totalRows <= 0 || totalColumns <= 0)
        return {};

    if (row < 0 || column < 0 || row >= totalRows || column >= totalColumns)
        return {};

    const SpatialReference webMercator = SpatialReference::webMercator();
    const SpatialReference wgs84 = SpatialReference::wgs84();

    const Point topLeft = projectToSpatialReference(pinnedImageCorners.at(0), webMercator);
    const Point topRight = projectToSpatialReference(pinnedImageCorners.at(1), webMercator);
    const Point bottomRight = projectToSpatialReference(pinnedImageCorners.at(2), webMercator);
    const Point bottomLeft = projectToSpatialReference(pinnedImageCorners.at(3), webMercator);

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
    const Point projected = geometry_cast<Point>(GeometryEngine::project(webPoint, wgs84));
    if (webPoint.isEmpty())
        return {};
    return Point(projected.x(), projected.y(), wgs84);
}
