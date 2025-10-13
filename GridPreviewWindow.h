#ifndef GRIDPREVIEWWINDOW_H
#define GRIDPREVIEWWINDOW_H

#include <QDialog>
#include <QPixmap>

class QLabel;
class QScrollArea;

class GridPreviewWindow : public QDialog
{
    Q_OBJECT
public:
    explicit GridPreviewWindow(QWidget *parent = nullptr);

    void setImageWithGrid(const QPixmap &pixmap, double widthMeters, double heightMeters);

private:
    QLabel *m_imageLabel = nullptr;
    QScrollArea *m_scrollArea = nullptr;
};

#endif // GRIDPREVIEWWINDOW_H
