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

signals:
    void effectCommitted(const QPixmap &pixmap, const QVector<QVector<int>> &seedChannels);

private:
    void updateDisplayedPixmap();
    QPixmap drawGridLines(const QPixmap &base) const;
    bool ensureEffectPixmaps();
    bool ensureOriginalGridImage();
    bool rebuildEffectPixmapsFromModifiedGrid();
    void updateButtonStates();
    void updateSeedItemNumbers();
    bool allSeedInputsValid() const;
    bool collectSeedDefinitions(QVector<SeedDefinition> &outSeeds) const;
    void connectSeedWidgetSignals(QWidget *widget);

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
    void updateColorSelectionUiState();

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
    QPushButton *m_showPaletteButton = nullptr;
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

    bool m_showEffect = true;
    bool m_showGridLines = true;
    bool m_highlightEmptyCells = false;

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
