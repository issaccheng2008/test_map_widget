#include "generate_path.h"

#include <QDebug>

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
