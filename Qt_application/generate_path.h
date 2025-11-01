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
                   const QVector<QVector<int>> &channelGrid,
                   const Esri::ArcGISRuntime::Point &currentGpsPoint);

QPair<double,double> gridCellGpsCoordinate(int row,
                                                 int column,
                                                 const QList<Esri::ArcGISRuntime::Point> &workAreaPolygon,
                                                 int totalRows,
                                                 int totalColumns);

#endif // GENERATE_PATH_H
