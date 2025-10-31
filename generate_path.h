#ifndef GENERATE_PATH_H
#define GENERATE_PATH_H

#include <QList>
#include <QVector>

namespace Esri::ArcGISRuntime {
class Geometry;
class Point;
}

extern const double car_width;
extern const double car_length;
extern const double grid_size;
extern const double max_image_area;
extern const int channel_number;

void generate_path(const QList<Esri::ArcGISRuntime::Point> &workAreaGps,
                   const QList<Esri::ArcGISRuntime::Geometry> &obstacles,
                   const QVector<QVector<int>> &channelGrid);

#endif // GENERATE_PATH_H
