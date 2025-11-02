#include "generate_path.h"

#include <QDebug>
#include <QStatusBar>

#include "Geometry.h"
#include "GeometryEngine.h"
#include "Point.h"
#include "SpatialReference.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <QJsonArray>
#include <QJsonObject>
#include <QLatin1Char>
#include <QStringList>
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

double calculateHeadingBetweenPoints(const Point &from, const Point &to)
{
    if (from.isEmpty() || to.isEmpty())
        return 0.0;

    constexpr double kDegToRad = 0.017453292519943295769; // pi / 180
    constexpr double kRadToDeg = 57.2957795130823208768;  // 180 / pi

    const double lat1 = from.y() * kDegToRad;
    const double lat2 = to.y() * kDegToRad;
    const double deltaLon = (to.x() - from.x()) * kDegToRad;

    const double y = std::sin(deltaLon) * std::cos(lat2);
    const double x = std::cos(lat1) * std::sin(lat2) - std::sin(lat1) * std::cos(lat2) * std::cos(deltaLon);

    double bearing = std::atan2(y, x) * kRadToDeg;
    if (bearing < 0.0)
        bearing += 360.0;

    return bearing;
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
const QString kEsp32BaseUrl = QStringLiteral("http://192.168.43.95");

struct data{
    Esri::ArcGISRuntime::Point coordinates;
    int state[7]{};
    double heading = 0.0;
};



PathGenerationResult generate_path(const QList<Esri::ArcGISRuntime::Point> &pinnedImageCorners,
                                   const QVector<QVector<int>> &channelGrid,
                                   const Esri::ArcGISRuntime::Point &currentGpsPoint,
                                   QStatusBar *statusBar)
{
    Q_UNUSED(statusBar);
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
    if(p.size()==0)return {pathCoordinates, QString(), QJsonArray()};

    //decide starting row
    double tt[2][2];
    for (int i=0;i<=1;i++)
        tt[0][i]=dis(pathCoordinates[0],gridCellGpsCoordinate(p.front()[0],p.front()[i],pinnedImageCorners,totalRows,totalColumns));
    for (int i=0;i<=1;i++)
        tt[1][i]=dis(pathCoordinates[0],gridCellGpsCoordinate(p.back()[0],p.back()[i],pinnedImageCorners,totalRows,totalColumns));

    int r=-1,ad=6,tim;
    if(std::min(tt[0][0],tt[0][1])>std::min(tt[1][0],tt[1][1]))
        std::reverse(p.begin(),p.end()),r=totalColumns,ad=-6;

    //generate path point
    Point p1,p2;
    QVector<data> channelinfo;

    for (int i=0,mini,maxi,inv;i<p.size();i++){
        r=p[i][0]+ad,mini=totalColumns-1,maxi=0,tim=1,inv=0;
        for (;i<p.size()&&(ad>0?p[i][0]<=r:p[i][0]>=r);i++)
            mini=std::min(mini,p[i][1]),maxi=std::max(maxi,p[i][2]);
        p1=gridCellGpsCoordinate(r-ad/2,mini,pinnedImageCorners,totalRows,totalColumns);
        p2=gridCellGpsCoordinate(r-ad/2,maxi,pinnedImageCorners,totalRows,totalColumns);

        if(dis(pathCoordinates.back(),p1)>dis(pathCoordinates.back(),p2)){
            std::swap(p1,p2),inv=1;
        }
        pathCoordinates.append(p1),pathCoordinates.append(p2);

        const double segmentHeading = calculateHeadingBetweenPoints(p1, p2);

//      generate channel release instructions
        for (int j=mini;j<=maxi;j++){
            data entry;
            entry.coordinates = gridCellGpsCoordinate(r-ad/2,j,pinnedImageCorners,totalRows,totalColumns);
            entry.heading = segmentHeading;
            if(ad<0)
                for (int k=std::max(r,0);k<=r+6;k++)
                    entry.state[r-k+6]=channelGrid[k][j];
            else{
                for (int k=std::min(totalColumns-1,r);k>=r-6;k--)
                    entry.state[k-r+6]=channelGrid[k][j];
            }
            if(inv)std::reverse(entry.state,entry.state+6);
            channelinfo.append(entry);
        }
    }

    // for (int i=0;i<channelinfo.size();i++){
    //     qDebug() << channelinfo[i].coordinates.x() << " " << channelinfo[i].coordinates.y();
    //     for (int j=0;j<=6;j++)qDebug() << channelinfo[i].state[j];
    // }

    QStringList trackPointLines;
    trackPointLines.reserve(pathCoordinates.size());
    for (const Point &coordinate : pathCoordinates) {
        if (coordinate.isEmpty())
            continue;
        const double latitude = coordinate.y();
        const double longitude = coordinate.x();
        const QString trackPointLine =
            QString("      <trkpt lat=\"%1\" lon=\"%2\" />").arg(latitude, 0, 'f', 10).arg(longitude, 0, 'f', 10);
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

    QJsonArray pathInfoArray;
    for (const data &entry : channelinfo) {
        if (entry.coordinates.isEmpty())
            continue;

        QJsonObject entryObject;
        entryObject.insert(QStringLiteral("lat"), entry.coordinates.y());
        entryObject.insert(QStringLiteral("lon"), entry.coordinates.x());
        entryObject.insert(QStringLiteral("heading"), entry.heading);

        QJsonArray stateArray;
        for (int i = 0; i < 7; ++i)
            stateArray.append(entry.state[i]);

        entryObject.insert(QStringLiteral("state"), stateArray);
        pathInfoArray.append(entryObject);
    }

    return {pathCoordinates, gpxDocument, pathInfoArray};
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
