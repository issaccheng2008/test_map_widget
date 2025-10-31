#include "generate_path.h"

#include <QDebug>

#include "Geometry.h"
#include "GeometryEngine.h"
#include "Point.h"
#include "SpatialReference.h"

#include <algorithm>

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

double clampFraction(double value)
{
    if (value < 0.0)
        return 0.0;
    if (value > 1.0)
        return 1.0;
    return value;
}

} // namespace

const double car_width = 0.6;
const double car_length = 1.5;
const double grid_size = 0.2;
const double max_image_area = 10000.0;
const int channel_number = 5;

void generate_path(const QList<Esri::ArcGISRuntime::Point> &workAreaPolygon,
                   const QList<QList<Esri::ArcGISRuntime::Point>> &obstacles,
                   const QVector<QVector<int>> &channelGrid)
{
    Q_UNUSED(workAreaPolygon)
    Q_UNUSED(obstacles)
    Q_UNUSED(channelGrid)

    qDebug() << "generate_path called";
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
                                      : clampFraction((static_cast<double>(column) + 0.5) / static_cast<double>(totalColumns));
    const double rowFraction = (totalRows == 1)
                                   ? 0.5
                                   : clampFraction((static_cast<double>(row) + 0.5) / static_cast<double>(totalRows));

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
