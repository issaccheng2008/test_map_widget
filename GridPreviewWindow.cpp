#include "GridPreviewWindow.h"

#include <QLabel>
#include <QPainter>
#include <QPen>
#include <QScrollArea>
#include <QVBoxLayout>

#include <cmath>

namespace
{
constexpr double kGridSpacingMeters = 0.3;
}

GridPreviewWindow::GridPreviewWindow(QWidget *parent)
    : QDialog(parent)
    , m_imageLabel(new QLabel(this))
    , m_scrollArea(new QScrollArea(this))
{
    setWindowTitle(tr("Pinned Image Grid"));
    setModal(false);

    m_scrollArea->setWidgetResizable(true);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_scrollArea->setWidget(m_imageLabel);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_scrollArea);

    resize(800, 600);
}

void GridPreviewWindow::setImageWithGrid(const QPixmap &pixmap, double widthMeters, double heightMeters)
{
    if (pixmap.isNull() || widthMeters <= 0.0 || heightMeters <= 0.0) {
        m_imageLabel->clear();
        return;
    }

    QPixmap gridPixmap = pixmap.copy();
    QPainter painter(&gridPixmap);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QPen(Qt::black, 1.0));

    const double horizontalSpacingPx = pixmap.width() * (kGridSpacingMeters / widthMeters);
    const double verticalSpacingPx = pixmap.height() * (kGridSpacingMeters / heightMeters);

    if (std::isfinite(horizontalSpacingPx) && horizontalSpacingPx > 0.5) {
        for (double x = horizontalSpacingPx; x < pixmap.width(); x += horizontalSpacingPx)
            painter.drawLine(QPointF(x, 0.0), QPointF(x, pixmap.height()));
    }

    if (std::isfinite(verticalSpacingPx) && verticalSpacingPx > 0.5) {
        for (double y = verticalSpacingPx; y < pixmap.height(); y += verticalSpacingPx)
            painter.drawLine(QPointF(0.0, y), QPointF(pixmap.width(), y));
    }

    painter.end();

    m_imageLabel->setPixmap(gridPixmap);
    m_imageLabel->adjustSize();
}
