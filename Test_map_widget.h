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
} // namespace Esri::ArcGISRuntime

#include <QMainWindow>
#include <QList>
#include <QPoint>
#include <QPixmap>
#include <QPointer>
#include <QVector>

#include <QtGlobal>
#include <optional>
#include <utility>

class OverlayImageWidget;
class GridPreviewWindow;

namespace Ui {
class Test_map_widget;
}


struct obstacles
{
    QList<Esri::ArcGISRuntime::Point> vertices;
};

extern QList<obstacles> obstaclesList;

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
    void setImagePosition();
    void openGridPreview();
    void applyCommittedGridEffect(const QPixmap &pixmap, const QVector<QVector<int>> &seedChannels);
    void startObstacleCapture();
    void finishObstacleCapture();
    void cancelObstacleCapture();

private:
    bool eventFilter(QObject *watched, QEvent *event) override;

    void addObstaclePoint(const QPoint &screenPoint);
    void rebuildObstaclePreview();
    void resetObstacleCreationState(bool keepActive);
    void updatePinnedImagePosition();
    void updateUiState();
    std::optional<QList<Esri::ArcGISRuntime::Point>> mapPointsForCurrentImageViewport() const;
    std::optional<double> currentImageAreaSquareMeters() const;
    bool isCurrentImageAreaAcceptable() const;
    std::optional<std::pair<double, double>> currentImageDimensionsMeters() const;
    std::optional<std::pair<double, double>> pinnedImageDimensionsMeters() const;
    std::optional<std::pair<double, double>> imageDimensionsMetersFromMapPoints(const QList<Esri::ArcGISRuntime::Point> &mapPoints) const;
    void updatePlacementInfoPanel(bool hasImage, bool hasPinnedImage);
    void updateCursorCoordinateDisplay(const QPoint &screenPoint);

    Esri::ArcGISRuntime::Map *m_map = nullptr;
    Esri::ArcGISRuntime::MapGraphicsView *m_mapView = nullptr;
    Esri::ArcGISRuntime::GraphicsOverlay *m_graphicsOverlay = nullptr;
    Esri::ArcGISRuntime::GraphicsOverlay *m_obstacleOverlay = nullptr;
    Esri::ArcGISRuntime::GraphicsOverlay *m_obstacleEditingOverlay = nullptr;
    OverlayImageWidget *m_imageOverlay = nullptr;

    QList<Esri::ArcGISRuntime::Point> m_currentObstaclePoints;
    bool m_isCapturingObstacle = false;
    QList<Esri::ArcGISRuntime::Point> m_pinnedImageMapPoints;
    bool m_isImagePinned = false;
    bool m_hasCommittedGridChanges = false;
    QPointer<GridPreviewWindow> m_gridWindow;
    QPixmap m_originalImagePixmap;
    quint64 m_imageSessionCounter = 0;
    quint64 m_currentImageSessionId = 0;
    quint64 m_gridWindowImageSessionId = 0;

    Ui::Test_map_widget *ui = nullptr;
};

#endif // TEST_MAP_WIDGET_H
