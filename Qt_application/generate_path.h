#ifndef GENERATE_PATH_H
#define GENERATE_PATH_H

#include <QList>
#include <QVector>
#include <QString>

#include "Point.h"

extern const double car_width;
extern const double car_length;
extern const double grid_size;
extern const double max_image_area;
extern const int channel_number;
extern const QString kEsp32BaseUrl;

class QStatusBar;

QVector<Esri::ArcGISRuntime::Point> generate_path(const QList<Esri::ArcGISRuntime::Point> &pinnedImageCorners,
                                                  const QVector<QVector<int>> &channelGrid,
                                                  const Esri::ArcGISRuntime::Point &currentGpsPoint,
                                                  QStatusBar *statusBar = nullptr);

Esri::ArcGISRuntime::Point gridCellGpsCoordinate(int row,
                                                 int column,
                                                 const QList<Esri::ArcGISRuntime::Point> &pinnedImageCorners,
                                                 int totalRows,
                                                 int totalColumns);

#endif // GENERATE_PATH_H
