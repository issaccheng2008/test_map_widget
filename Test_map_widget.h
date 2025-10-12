// Copyright 2025 ESRI
//
// All rights reserved under the copyright laws of the United States
// and applicable international laws, treaties, and conventions.
//
// You may freely redistribute and use this sample code, with or
// without modification, provided you include the original copyright
// notice and use restrictions.
//
// See the Sample code usage restrictions document for further information.
//

#ifndef TEST_MAP_WIDGET_H
#define TEST_MAP_WIDGET_H

namespace Esri::ArcGISRuntime {
class Map;
class MapGraphicsView;
class Graphic;
class GraphicsOverlay;
class Point;
} // namespace Esri::ArcGISRuntime

#include <QMainWindow>

namespace Ui {
class Test_map_widget;
}

class Test_map_widget : public QMainWindow
{
    Q_OBJECT
public:
    explicit Test_map_widget(QWidget *parent = nullptr);
    ~Test_map_widget() override;

private slots:
    void goToCoordinates();
    void drawLineFromInput();

private:
    void drawLineBetweenPoints(const Esri::ArcGISRuntime::Point &start,
                               const Esri::ArcGISRuntime::Point &end);

    Esri::ArcGISRuntime::Map *m_map = nullptr;
    Esri::ArcGISRuntime::MapGraphicsView *m_mapView = nullptr;
    Esri::ArcGISRuntime::GraphicsOverlay *m_graphicsOverlay = nullptr;
    Esri::ArcGISRuntime::Graphic *m_routeGraphic = nullptr;
    Ui::Test_map_widget *ui = nullptr;
};

#endif // TEST_MAP_WIDGET_H
