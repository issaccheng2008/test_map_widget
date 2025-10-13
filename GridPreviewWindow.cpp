#include "GridPreviewWindow.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <cmath>
#include <algorithm>

namespace
{
constexpr double kGridSpacingMeters = 0.3;
}

GridPreviewWindow::GridPreviewWindow(QWidget *parent)
    : QDialog(parent)
    , m_imageLabel(new QLabel(this))
    , m_scrollArea(new QScrollArea(this))
    , m_seeEffectButton(new QPushButton(tr("See effect"), this))
    , m_toggleGridLinesButton(new QPushButton(tr("Hide grid lines"), this))
    , m_commitButton(new QPushButton(tr("Commit changes"), this))
{
    setWindowTitle(tr("Pinned Image Grid"));
    setModal(false);

    m_scrollArea->setWidgetResizable(true);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_scrollArea->setWidget(m_imageLabel);

    auto *mainLayout = new QVBoxLayout(this);
    auto *contentLayout = new QHBoxLayout();
    auto *sideLayout = new QVBoxLayout();

    sideLayout->addWidget(m_seeEffectButton);
    sideLayout->addWidget(m_toggleGridLinesButton);
    sideLayout->addStretch(1);

    contentLayout->addWidget(m_scrollArea, 1);
    contentLayout->addLayout(sideLayout);

    auto *bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch(1);
    bottomLayout->addWidget(m_commitButton);

    mainLayout->addLayout(contentLayout);
    mainLayout->addLayout(bottomLayout);

    connect(m_seeEffectButton, &QPushButton::clicked, this, &GridPreviewWindow::handleSeeEffectClicked);
    connect(m_toggleGridLinesButton, &QPushButton::clicked, this, &GridPreviewWindow::handleToggleGridLinesClicked);
    connect(m_commitButton, &QPushButton::clicked, this, &GridPreviewWindow::handleCommitClicked);

    updateButtonStates();

    resize(900, 650);
}

void GridPreviewWindow::setImageWithGrid(const QPixmap &pixmap, double widthMeters, double heightMeters)
{
    m_originalPixmap = pixmap;
    m_originalWithGridPixmap = {};
    m_effectPixmap = {};
    m_effectWithGridPixmap = {};
    m_showEffect = false;
    m_showGridLines = true;
    m_cellWidthPx = 0;
    m_cellHeightPx = 0;

    if (pixmap.isNull() || widthMeters <= 0.0 || heightMeters <= 0.0) {
        m_imageLabel->clear();
        updateButtonStates();
        return;
    }

    const double horizontalSpacingPx = pixmap.width() * (kGridSpacingMeters / widthMeters);
    const double verticalSpacingPx = pixmap.height() * (kGridSpacingMeters / heightMeters);

    if (std::isfinite(horizontalSpacingPx) && horizontalSpacingPx >= 1.0)
        m_cellWidthPx = std::max(1, static_cast<int>(std::round(horizontalSpacingPx)));
    if (std::isfinite(verticalSpacingPx) && verticalSpacingPx >= 1.0)
        m_cellHeightPx = std::max(1, static_cast<int>(std::round(verticalSpacingPx)));

    if (m_cellWidthPx <= 0 || m_cellHeightPx <= 0) {
        m_cellWidthPx = 0;
        m_cellHeightPx = 0;
    }

    m_originalWithGridPixmap = drawGridLines(m_originalPixmap);
    updateDisplayedPixmap();
}

void GridPreviewWindow::updateDisplayedPixmap()
{
    QPixmap displayPixmap;

    if (m_showEffect) {
        if (!ensureEffectPixmaps())
            m_showEffect = false;
        else
            displayPixmap = m_showGridLines ? m_effectWithGridPixmap : m_effectPixmap;
    }

    if (!m_showEffect) {
        displayPixmap = m_showGridLines ? m_originalWithGridPixmap : m_originalPixmap;
    }

    if (displayPixmap.isNull())
        m_imageLabel->clear();
    else
        m_imageLabel->setPixmap(displayPixmap);

    m_imageLabel->adjustSize();
    updateButtonStates();
}

QPixmap GridPreviewWindow::drawGridLines(const QPixmap &base) const
{
    if (base.isNull() || m_cellWidthPx <= 0 || m_cellHeightPx <= 0)
        return base;

    QPixmap result = base;
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QPen(Qt::black, 1.0));

    for (int x = m_cellWidthPx; x < result.width(); x += m_cellWidthPx)
        painter.drawLine(QPointF(x, 0.0), QPointF(x, result.height()));

    for (int y = m_cellHeightPx; y < result.height(); y += m_cellHeightPx)
        painter.drawLine(QPointF(0.0, y), QPointF(result.width(), y));

    painter.end();
    return result;
}

bool GridPreviewWindow::ensureEffectPixmaps()
{
    if (!m_effectPixmap.isNull() || !m_effectWithGridPixmap.isNull())
        return true;

    if (m_originalPixmap.isNull() || m_cellWidthPx <= 0 || m_cellHeightPx <= 0)
        return false;

    const int columns = m_originalPixmap.width() / m_cellWidthPx;
    const int rows = m_originalPixmap.height() / m_cellHeightPx;

    if (columns <= 0 || rows <= 0)
        return false;

    const int cropWidth = columns * m_cellWidthPx;
    const int cropHeight = rows * m_cellHeightPx;

    QImage sourceImage = m_originalPixmap.toImage().convertToFormat(QImage::Format_ARGB32);
    if (sourceImage.width() < cropWidth || sourceImage.height() < cropHeight)
        return false;

    QImage effectImage(cropWidth, cropHeight, QImage::Format_ARGB32);
    effectImage.fill(Qt::transparent);

    QPainter painter(&effectImage);
    painter.setPen(Qt::NoPen);

    const int pixelsPerCell = m_cellWidthPx * m_cellHeightPx;

    for (int row = 0; row < rows; ++row) {
        const int yStart = row * m_cellHeightPx;
        for (int column = 0; column < columns; ++column) {
            const int xStart = column * m_cellWidthPx;

            quint64 sumRed = 0;
            quint64 sumGreen = 0;
            quint64 sumBlue = 0;
            quint64 sumAlpha = 0;

            for (int y = 0; y < m_cellHeightPx; ++y) {
                const QRgb *line = reinterpret_cast<const QRgb *>(sourceImage.constScanLine(yStart + y));
                for (int x = 0; x < m_cellWidthPx; ++x) {
                    const QRgb pixel = line[xStart + x];
                    const int alpha = qAlpha(pixel);
                    sumAlpha += alpha;
                    sumRed += static_cast<quint64>(qRed(pixel)) * alpha;
                    sumGreen += static_cast<quint64>(qGreen(pixel)) * alpha;
                    sumBlue += static_cast<quint64>(qBlue(pixel)) * alpha;
                }
            }

            int averageAlpha = pixelsPerCell > 0 ? static_cast<int>(sumAlpha / pixelsPerCell) : 0;
            averageAlpha = std::clamp(averageAlpha, 0, 255);

            int averageRed = 0;
            int averageGreen = 0;
            int averageBlue = 0;

            if (sumAlpha > 0) {
                averageRed = static_cast<int>(sumRed / sumAlpha);
                averageGreen = static_cast<int>(sumGreen / sumAlpha);
                averageBlue = static_cast<int>(sumBlue / sumAlpha);
            }

            averageRed = std::clamp(averageRed, 0, 255);
            averageGreen = std::clamp(averageGreen, 0, 255);
            averageBlue = std::clamp(averageBlue, 0, 255);

            painter.setBrush(QColor(averageRed, averageGreen, averageBlue, averageAlpha));
            painter.drawRect(QRect(xStart, yStart, m_cellWidthPx, m_cellHeightPx));
        }
    }

    painter.end();

    m_effectPixmap = QPixmap::fromImage(effectImage);
    m_effectWithGridPixmap = drawGridLines(m_effectPixmap);

    return !m_effectPixmap.isNull();
}

void GridPreviewWindow::updateButtonStates()
{
    const bool hasOriginal = !m_originalPixmap.isNull();
    const bool effectPossible = hasOriginal && m_cellWidthPx > 0 && m_cellHeightPx > 0 &&
                                 (m_originalPixmap.width() / m_cellWidthPx) > 0 &&
                                 (m_originalPixmap.height() / m_cellHeightPx) > 0;

    if (m_seeEffectButton) {
        m_seeEffectButton->setEnabled(effectPossible || m_showEffect);
        m_seeEffectButton->setText(m_showEffect ? tr("Show original") : tr("See effect"));
    }

    if (m_toggleGridLinesButton) {
        const bool hasPixmap = (m_showEffect && !m_effectPixmap.isNull()) || (!m_showEffect && hasOriginal);
        m_toggleGridLinesButton->setEnabled(hasPixmap);
        m_toggleGridLinesButton->setText(m_showGridLines ? tr("Hide grid lines") : tr("Show grid lines"));
    }

    if (m_commitButton)
        m_commitButton->setEnabled(effectPossible);
}

void GridPreviewWindow::handleSeeEffectClicked()
{
    if (!m_showEffect) {
        if (!ensureEffectPixmaps())
            return;
        m_showEffect = true;
    } else {
        m_showEffect = false;
    }

    updateDisplayedPixmap();
}

void GridPreviewWindow::handleToggleGridLinesClicked()
{
    m_showGridLines = !m_showGridLines;
    updateDisplayedPixmap();
}

void GridPreviewWindow::handleCommitClicked()
{
    if (!ensureEffectPixmaps())
        return;

    emit effectCommitted(m_effectPixmap);
    close();
}
