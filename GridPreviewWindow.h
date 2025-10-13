#ifndef GRIDPREVIEWWINDOW_H
#define GRIDPREVIEWWINDOW_H

#include <QDialog>
#include <QPixmap>

class QLabel;
class QScrollArea;
class QPushButton;

class GridPreviewWindow : public QDialog
{
    Q_OBJECT
public:
    explicit GridPreviewWindow(QWidget *parent = nullptr);

    void setImageWithGrid(const QPixmap &pixmap, double widthMeters, double heightMeters);

signals:
    void effectCommitted(const QPixmap &pixmap);

private:
    void updateDisplayedPixmap();
    QPixmap drawGridLines(const QPixmap &base) const;
    bool ensureEffectPixmaps();
    void updateButtonStates();

    void handleSeeEffectClicked();
    void handleToggleGridLinesClicked();
    void handleCommitClicked();

    QLabel *m_imageLabel = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    QPushButton *m_seeEffectButton = nullptr;
    QPushButton *m_toggleGridLinesButton = nullptr;
    QPushButton *m_commitButton = nullptr;

    QPixmap m_originalPixmap;
    QPixmap m_originalWithGridPixmap;
    QPixmap m_effectPixmap;
    QPixmap m_effectWithGridPixmap;

    bool m_showEffect = false;
    bool m_showGridLines = true;

    int m_cellWidthPx = 0;
    int m_cellHeightPx = 0;
};

#endif // GRIDPREVIEWWINDOW_H
