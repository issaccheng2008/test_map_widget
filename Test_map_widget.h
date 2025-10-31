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
class GraphicsOverlay;
class Point;
class Geometry;
} // namespace Esri::ArcGISRuntime

#include <QMainWindow>
#include <QList>
#include <QPolygonF>

class OverlayImageWidget;
class SeedSelectionWindow;

namespace Ui {
class Test_map_widget;
}

class Test_map_widget : public QMainWindow
{
    Q_OBJECT
public:
    explicit Test_map_widget(QWidget *parent = nullptr);
    ~Test_map_widget() override;

    void drawLineBetweenCoordinates(const Esri::ArcGISRuntime::Point &start,
                                    const Esri::ArcGISRuntime::Point &end);

private slots:
    void goToCoordinates();
    void importImage();
    void clearImportedImage();
    void toggleWorkArea();
    void openSeedSelectionWindow();
    void generatePath();
    void onSeedsChanged();
    void onOverlayImageLoaded();
    void onOverlayImageCleared();

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void updateImageDependentUi();
    void updateGeneratePathButtonState();
    void hideWorkAreaGraphic();
    QList<Esri::ArcGISRuntime::Point> workAreaCoordinates() const;
    Esri::ArcGISRuntime::Geometry buildWorkAreaGeometry() const;

    Esri::ArcGISRuntime::Map *m_map = nullptr;
    Esri::ArcGISRuntime::MapGraphicsView *m_mapView = nullptr;
    Esri::ArcGISRuntime::GraphicsOverlay *m_graphicsOverlay = nullptr;
    Esri::ArcGISRuntime::GraphicsOverlay *m_workAreaOverlay = nullptr;
    OverlayImageWidget *m_imageOverlay = nullptr;
    Esri::ArcGISRuntime::Graphic *m_workAreaGraphic = nullptr;
    SeedSelectionWindow *m_seedSelectionWindow = nullptr;
    QList<Esri::ArcGISRuntime::Geometry> m_obstacles;
    bool m_isWorkAreaVisible = false;
    Ui::Test_map_widget *ui = nullptr;
};

#endif // TEST_MAP_WIDGET_H
