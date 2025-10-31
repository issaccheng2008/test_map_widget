#ifndef GENERATE_PATH_H
#define GENERATE_PATH_H

#include <QList>
#include <QVector>

#include "Point.h"

extern const double car_width;
extern const double car_length;
extern const double grid_size;
extern const double max_image_area;
extern const int channel_number;

void generate_path(const QList<Esri::ArcGISRuntime::Point> &workAreaPolygon,
                   const QList<QList<Esri::ArcGISRuntime::Point>> &obstacles,
                   const QVector<QVector<int>> &channelGrid);

#endif // GENERATE_PATH_H
