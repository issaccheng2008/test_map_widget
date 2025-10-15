#ifndef GRIDPREVIEWWINDOW_H
#define GRIDPREVIEWWINDOW_H

#include <QColor>
#include <QDialog>
#include <QImage>
#include <QPixmap>
#include <QVector>

class QLabel;
class QScrollArea;
class QPushButton;
class QListWidget;

class GridPreviewWindow : public QDialog
{
    Q_OBJECT
public:
    explicit GridPreviewWindow(QWidget *parent = nullptr);

    void setImageWithGrid(const QPixmap &pixmap, double widthMeters, double heightMeters);

signals:
    void effectCommitted(const QPixmap &pixmap, const QVector<QVector<int>> &seedChannels);

private:
    struct SeedDefinition
    {
        QColor seedColor;
        QColor targetColor;
        double weight = 1.0;
        int channel = 0;

        [[nodiscard]] bool channelIsEmpty() const { return channel == 0; }
    };

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

    void handleSeeEffectPressed();
    void handleSeeEffectReleased();
    void handleToggleGridLinesClicked();
    void handleCommitClicked();

    QLabel *m_imageLabel = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    QPushButton *m_seeEffectButton = nullptr;
    QPushButton *m_toggleGridLinesButton = nullptr;
    QPushButton *m_commitButton = nullptr;
    QPushButton *m_addSeedButton = nullptr;
    QPushButton *m_deleteSeedButton = nullptr;
    QPushButton *m_applyChangesButton = nullptr;
    QListWidget *m_seedListWidget = nullptr;

    QPixmap m_originalPixmap;
    QPixmap m_originalWithGridPixmap;
    QPixmap m_effectPixmap;
    QPixmap m_effectWithGridPixmap;
    QImage m_originalGridImage;
    QImage m_modifiedGridImage;
    QVector<QVector<int>> m_appliedSeedChannels;

    bool m_showEffect = false;
    bool m_showGridLines = true;
    bool m_shouldRestoreEffectAfterPress = false;

    int m_cellWidthPx = 0;
    int m_cellHeightPx = 0;
    int m_gridColumns = 0;
    int m_gridRows = 0;
};

#endif // GRIDPREVIEWWINDOW_H
