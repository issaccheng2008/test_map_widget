#ifndef GRIDSTATE_H
#define GRIDSTATE_H

#include <QPolygonF>
#include <QVector>

extern QVector<QVector<int>> g_channelGrid;
extern QPolygonF g_pinnedImageFootprint;

inline constexpr double kGridSpacingMeters = 0.3;

#endif // GRIDSTATE_H
