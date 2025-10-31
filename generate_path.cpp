#include "generate_path.h"

#include <QDebug>

#include "Geometry.h"
#include "GeometryEngine.h"
#include "Point.h"
#include "SpatialReference.h"

#include <algorithm>
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
                   const QVector<QVector<int>> &channelGrid)
{
    Q_UNUSED(obstacles)

    qDebug() << "generate_path called";

    const int totalRows = channelGrid.size();
    int totalColumns = 0;
    for (const QVector<int> &row : channelGrid) {
        if (!row.isEmpty()) {
            totalColumns = row.size();
            break;
        }
    }

    if (polygonHasRequiredCorners(workAreaPolygon) && totalRows > 0 && totalColumns > 0) {
        const Point example = gridCellGpsCoordinate(0, 0, workAreaPolygon, totalRows, totalColumns);
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

    const QVector2D widthVector(topRight.x() - topLeft.x(), topRight.y() - topLeft.y());
    const QVector2D heightVector(bottomLeft.x() - topLeft.x(), bottomLeft.y() - topLeft.y());

    const double widthLength = widthVector.length();
    const double heightLength = heightVector.length();

    if (widthLength <= 0.0 || heightLength <= 0.0)
        return {};

    const double availableWidth = widthLength - 2.0 * car_width;
    const double availableHeight = heightLength - 2.0 * car_width;

    if (availableWidth <= 0.0 || availableHeight <= 0.0)
        return {};

    const QVector2D widthDirection = widthVector / widthLength;
    const QVector2D heightDirection = heightVector / heightLength;

    const QVector2D gridOriginOffset = widthDirection * car_width + heightDirection * car_width;

    const double cellWidth = availableWidth / static_cast<double>(totalColumns);
    const double cellHeight = availableHeight / static_cast<double>(totalRows);

    const QVector2D cellOffset = widthDirection * (cellWidth * (static_cast<double>(column) + 0.5)) +
                                 heightDirection * (cellHeight * (static_cast<double>(row) + 0.5));

    const double interpolatedX = topLeft.x() + gridOriginOffset.x() + cellOffset.x();
    const double interpolatedY = topLeft.y() + gridOriginOffset.y() + cellOffset.y();

    const Point webPoint(interpolatedX, interpolatedY, webMercator);
    if (webPoint.isEmpty())
        return {};

    return geometry_cast<Point>(GeometryEngine::project(webPoint, wgs84));
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
