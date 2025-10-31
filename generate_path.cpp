#include "generate_path.h"

#include <QDebug>

#include "Geometry.h"
#include "Point.h"

const double car_width = 0.6;
const double car_length = 1.5;
const double grid_size = 0.2;
const double max_image_area = 10000.0;
const int channel_number = 12;

void generate_path(const QList<Esri::ArcGISRuntime::Point> &workAreaGps,
                   const QList<Esri::ArcGISRuntime::Geometry> &obstacles,
                   const QVector<QVector<int>> &channelGrid)
{
    qDebug() << "generate_path invoked";
    qDebug() << "car_width:" << car_width << "car_length:" << car_length;
    qDebug() << "grid_size:" << grid_size << "max_image_area:" << max_image_area;
    qDebug() << "work area vertices:" << workAreaGps.size();
    qDebug() << "obstacle count:" << obstacles.size();
    qDebug() << "channel grid rows:" << channelGrid.size();
    if (!channelGrid.isEmpty())
        qDebug() << "first row channels:" << channelGrid.first();
}
