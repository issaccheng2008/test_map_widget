#ifndef GENERATE_PATH_H
#define GENERATE_PATH_H

#include <QJsonArray>
#include <QList>
#include <QString>
#include <QVector>

#include "Point.h"

extern const double car_width;
extern const double car_length;
extern const double grid_size;
extern const double max_image_area;
extern const int channel_number;
extern const QString kEsp32BaseUrl;

class QStatusBar;

struct PathGenerationResult
{
    QVector<Esri::ArcGISRuntime::Point> pathPoints;
    QString gpxDocument;
    QJsonArray pathInfoArray;

    bool hasDrawablePath() const { return pathPoints.size() >= 2; }
    bool hasPayload() const { return !gpxDocument.isEmpty(); }
};

PathGenerationResult generate_path(const QList<Esri::ArcGISRuntime::Point> &pinnedImageCorners,
                                   const QVector<QVector<int>> &channelGrid,
                                   const Esri::ArcGISRuntime::Point &currentGpsPoint,
                                   QStatusBar *statusBar = nullptr);

Esri::ArcGISRuntime::Point gridCellGpsCoordinate(int row,
                                                 int column,
                                                 const QList<Esri::ArcGISRuntime::Point> &pinnedImageCorners,
                                                 int totalRows,
                                                 int totalColumns);

#endif // GENERATE_PATH_H
