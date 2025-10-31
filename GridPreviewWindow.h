#ifndef GRIDPREVIEWWINDOW_H
#define GRIDPREVIEWWINDOW_H

#include <QColor>
#include <QDialog>
#include <QImage>
#include <QPixmap>
#include <QVector>

#include <memory>

class ColorPickerOverlay;

class QLabel;
class QScrollArea;
class QPushButton;
class QListWidget;
class QLineEdit;
class QWidget;
class QFrame;
class QVBoxLayout;
class QResizeEvent;

class GridPreviewWindow : public QDialog
{
    Q_OBJECT
public:
    struct SeedDefinition
    {
        QColor seedColor;
        QColor targetColor;
        double weight = 1.0;
        int channel = 0;

        [[nodiscard]] bool channelIsEmpty() const { return channel == 0; }
    };

    explicit GridPreviewWindow(QWidget *parent = nullptr);
    ~GridPreviewWindow() override;

    void setImageWithGrid(const QPixmap &pixmap, double widthMeters, double heightMeters);
    void resetState();
    [[nodiscard]] bool hasSession() const;

signals:
    void effectCommitted(const QPixmap &pixmap, const QVector<QVector<int>> &seedChannels);

private:
    void updateDisplayedPixmap();
    QPixmap drawGridLines(const QPixmap &base) const;
    bool ensureEffectPixmaps();
    bool ensureOriginalGridImage();
    bool rebuildEffectPixmapsFromModifiedGrid();
    void updateButtonStates();
    void updateAddSeedButtonState();
    void updateChannelAvailability();
    void invalidateGeneratedEffect();
    void updateSeedItemNumbers();
    bool allSeedInputsValid() const;
    bool collectSeedDefinitions(QVector<SeedDefinition> &outSeeds) const;
    void connectSeedWidgetSignals(QWidget *widget);
    int lowestAvailableChannel(int excludeRow) const;

    void handleAddSeedClicked();
    void handleDeleteSeedClicked();
    void handleApplyChangesClicked();

    void handleSeeEffectClicked();
    void handleToggleGridLinesClicked();
    void handleCommitClicked();
    void handlePaletteButtonClicked();

    void startColorPickingForWidget(QLineEdit *lineEdit, QLabel *previewLabel);
    void stopColorPicking();
    void applyEmptyChannelHighlight(QImage &image) const;
    void applyObstacleHighlight(QImage &image, bool drawFill) const;
    void drawCellHighlightsForValue(QImage &image, int cellValue, const QColor &fillColor, const QColor &edgeColor,
                                    bool drawFill) const;
    void applyObstacleMaskToGrid(QVector<QVector<int>> &grid) const;
    void applyObstacleTransparencyToImage(QImage &image) const;
    void updateObstacleMask();
    bool cellHasObstacle(int row, int column) const;
    void updateColorSelectionUiState();
    void updatePalettePanelGeometry();

    void resizeEvent(QResizeEvent *event) override;

    QLabel *m_imageLabel = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    QPushButton *m_seeEffectButton = nullptr;
    QPushButton *m_toggleGridLinesButton = nullptr;
    QPushButton *m_commitButton = nullptr;
    QPushButton *m_addSeedButton = nullptr;
    QPushButton *m_deleteSeedButton = nullptr;
    QPushButton *m_applyChangesButton = nullptr;
    QListWidget *m_seedListWidget = nullptr;
    QWidget *m_paletteContainer = nullptr;
    QFrame *m_paletteFrame = nullptr;
    QVBoxLayout *m_paletteLayout = nullptr;
    QPushButton *m_showPaletteButton = nullptr;
    QPushButton *m_exitColorSelectionButton = nullptr;
    QLabel *m_paletteImageLabel = nullptr;

    QPixmap m_originalPixmap;
    QPixmap m_originalWithGridPixmap;
    QPixmap m_effectPixmap;
    QPixmap m_effectWithGridPixmap;
    QPixmap m_effectPreviewPixmap;
    QPixmap m_effectPreviewWithGridPixmap;
    QImage m_originalGridImage;
    QImage m_modifiedGridImage;
    QVector<QVector<int>> m_appliedSeedChannels;
    QVector<QVector<bool>> m_obstacleCellsMask;

    bool m_showEffect = true;
    bool m_showGridLines = true;
    bool m_highlightEmptyCells = false;
    bool m_hasGeneratedEffect = false;

    int m_cellWidthPx = 0;
    int m_cellHeightPx = 0;
    int m_gridColumns = 0;
    int m_gridRows = 0;

    std::unique_ptr<ColorPickerOverlay> m_colorPickerOverlay;
    QLineEdit *m_activeColorLineEdit = nullptr;
    QLabel *m_activeColorPreview = nullptr;
    QString m_activePreviewOriginalStyle;
    bool m_paletteVisible = false;
}; 

#endif // GRIDPREVIEWWINDOW_H
