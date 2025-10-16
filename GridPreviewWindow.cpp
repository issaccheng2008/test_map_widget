#include "GridPreviewWindow.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QComboBox>
#include <QColor>
#include <QCursor>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPalette>
#include <QPushButton>
#include <QPointer>
#include <QScreen>
#include <QResizeEvent>
#include <QMargins>
#include <QScrollArea>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QVariant>
#include <QStringList>

#include <cmath>
#include <algorithm>
#include <functional>
#include <limits>
#include <optional>

namespace
{
constexpr double kGridSpacingMeters = 0.3;
constexpr int kChannelCount = 5;

QCursor createDropperCursor()
{
    QPixmap resourcePixmap(QStringLiteral(":/color-picker-dropper-colour-svgrepo-com.png"));
    if (!resourcePixmap.isNull()) {
        const QSize desiredSize(32, 32);
        resourcePixmap = resourcePixmap.scaled(desiredSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        const QPoint hotSpot(resourcePixmap.width() - 4, resourcePixmap.height() - 4);
        return QCursor(resourcePixmap, hotSpot.x(), hotSpot.y());
    }

    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPen outlinePen(QColor(32, 32, 32));
    outlinePen.setWidth(2);
    outlinePen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(outlinePen);
    painter.setBrush(QColor(240, 240, 240));

    QPainterPath path;
    path.moveTo(6, 20);
    path.lineTo(10, 16);
    path.lineTo(16, 10);
    path.cubicTo(19, 7, 19, 4, 17, 2);
    path.cubicTo(15, 0, 12, 1, 10, 3);
    path.lineTo(4, 9);
    path.lineTo(2, 11);
    path.lineTo(6, 15);
    path.lineTo(2, 19);
    path.closeSubpath();
    painter.drawPath(path);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(120, 180, 255));
    painter.drawEllipse(QPointF(15.5, 8.5), 3.0, 3.0);

    painter.end();

    return QCursor(pixmap, 6, 20);
}
} // namespace

class ColorPickerOverlay : public QWidget
{
public:
    explicit ColorPickerOverlay(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents, false);
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setWindowFlag(Qt::FramelessWindowHint, true);
        setWindowFlag(Qt::Tool, true);
        setWindowFlag(Qt::WindowStaysOnTopHint, true);
        setWindowFlag(Qt::BypassWindowManagerHint, true);
        setMouseTracking(true);
    }

    ~ColorPickerOverlay() override
    {
        finish();
    }

    void begin()
    {
        QRect bounds;
        const QList<QScreen *> screens = QGuiApplication::screens();
        for (QScreen *screen : screens) {
            if (screen)
                bounds = bounds.united(screen->geometry());
        }

        if (bounds.isEmpty()) {
            if (QScreen *primary = QGuiApplication::primaryScreen())
                bounds = primary->geometry();
        }

        if (bounds.isEmpty())
            bounds = QRect(QPoint(0, 0), QSize(1, 1));

        setGeometry(bounds);
        show();
        raise();
        activateWindow();

        if (!m_cursorActive) {
            QGuiApplication::setOverrideCursor(createDropperCursor());
            m_cursorActive = true;
        }

        grabMouse();
        grabKeyboard();
    }

    void finish()
    {
        if (m_cursorActive) {
            QGuiApplication::restoreOverrideCursor();
            m_cursorActive = false;
        }

        releaseMouse();
        releaseKeyboard();
        hide();
    }

    void setColorPickedCallback(std::function<void(const QColor &)> callback)
    {
        m_colorPickedCallback = std::move(callback);
    }

    void setCanceledCallback(std::function<void()> callback)
    {
        m_cancelCallback = std::move(callback);
    }

    void setOriginWidget(QWidget *widget)
    {
        m_originWidget = widget;
    }

    void setPaletteButton(QAbstractButton *button)
    {
        m_paletteButton = button;
    }

    void setExitButton(QAbstractButton *button)
    {
        m_exitButton = button;
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (!event)
            return;

        const QPoint globalPos = event->globalPosition().toPoint();

        if (event->button() == Qt::LeftButton) {
            if (m_paletteButton && m_paletteButton->isEnabled() && m_paletteButton->isVisible()) {
                const QRect buttonRect(m_paletteButton->mapToGlobal(QPoint(0, 0)), m_paletteButton->size());
                if (buttonRect.contains(globalPos)) {
                    m_paletteButton->animateClick();
                    event->accept();
                    return;
                }
            }

            if (m_exitButton && m_exitButton->isEnabled() && m_exitButton->isVisible()) {
                const QRect buttonRect(m_exitButton->mapToGlobal(QPoint(0, 0)), m_exitButton->size());
                if (buttonRect.contains(globalPos)) {
                    m_exitButton->animateClick();
                    event->accept();
                    return;
                }
            }

            event->accept();
            return;
        }

        if (event->button() == Qt::RightButton) {
            const QColor picked = grabColorAt(globalPos);
            if (picked.isValid()) {
                if (m_colorPickedCallback)
                    m_colorPickedCallback(picked);
            }
            event->accept();
            return;
        }

        QWidget::mousePressEvent(event);
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        if (event && event->key() == Qt::Key_Escape) {
            event->accept();
            return;
        }

        QWidget::keyPressEvent(event);
    }

    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);
        QPainter painter(this);
        painter.fillRect(rect(), QColor(0, 0, 0, 0));
    }

private:
    [[nodiscard]] QColor grabColorAt(const QPoint &globalPos) const
    {
        QScreen *screen = QGuiApplication::screenAt(globalPos);
        if (!screen)
            screen = QGuiApplication::primaryScreen();

        if (!screen)
            return QColor();

        QPixmap pixmap = screen->grabWindow(0, globalPos.x(), globalPos.y(), 1, 1);
        if (pixmap.isNull())
            return QColor();

        QImage image = pixmap.toImage();
        if (image.isNull())
            return QColor();

        return image.pixelColor(0, 0);
    }

    std::function<void(const QColor &)> m_colorPickedCallback;
    std::function<void()> m_cancelCallback;
    QPointer<QWidget> m_originWidget;
    QPointer<QAbstractButton> m_paletteButton;
    QPointer<QAbstractButton> m_exitButton;
    bool m_cursorActive = false;
};

namespace
{
class ColorPreviewLabel : public QLabel
{
public:
    explicit ColorPreviewLabel(QWidget *parent = nullptr)
        : QLabel(parent)
    {
        setCursor(Qt::PointingHandCursor);
    }

    void setClickCallback(std::function<void()> callback)
    {
        m_callback = std::move(callback);
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event && event->button() == Qt::LeftButton) {
            if (m_callback)
                m_callback();
            event->accept();
            return;
        }

        QLabel::mousePressEvent(event);
    }

private:
    std::function<void()> m_callback;
};

class SeedItemWidget : public QWidget
{
public:
    explicit SeedItemWidget(int itemNumber, QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setupUi();
        setItemNumber(itemNumber);
    }

    void setItemNumber(int number)
    {
        if (m_numberLabel)
            m_numberLabel->setText(QString::number(number));
    }

    [[nodiscard]] bool channelIsEmpty() const
    {
        return m_channelCombo && m_channelCombo->currentIndex() == 0;
    }

    [[nodiscard]] bool hasValidSeedColor() const
    {
        if (channelIsEmpty())
            return true;

        bool ok = false;
        parseColorString(m_seedColorEdit ? m_seedColorEdit->text() : QString(), &ok);
        return ok;
    }

    [[nodiscard]] bool hasValidTargetColor() const
    {
        bool ok = false;
        parseColorString(m_targetColorEdit ? m_targetColorEdit->text() : QString(), &ok);
        return ok;
    }

    [[nodiscard]] bool hasValidWeight() const
    {
        if (!m_targetWeightEdit)
            return false;

        bool ok = false;
        const double value = m_targetWeightEdit->text().trimmed().toDouble(&ok);
        return ok && value > 0.0;
    }

    [[nodiscard]] QColor seedColorValue(bool *ok = nullptr) const
    {
        if (channelIsEmpty()) {
            if (ok)
                *ok = true;
            return QColor(0, 0, 0, 0);
        }

        return parseColorString(m_seedColorEdit ? m_seedColorEdit->text() : QString(), ok);
    }

    [[nodiscard]] QColor targetColorValue(bool *ok = nullptr) const
    {
        return parseColorString(m_targetColorEdit ? m_targetColorEdit->text() : QString(), ok);
    }

    [[nodiscard]] double weightValue(bool *ok = nullptr) const
    {
        if (!m_targetWeightEdit) {
            if (ok)
                *ok = false;
            return 0.0;
        }

        bool localOk = false;
        const double value = m_targetWeightEdit->text().trimmed().toDouble(&localOk);
        const bool valid = localOk && value > 0.0;
        if (ok)
            *ok = valid;
        return valid ? value : 0.0;
    }

    [[nodiscard]] int channelValue() const
    {
        if (!m_channelCombo)
            return 0;

        const QVariant data = m_channelCombo->currentData();
        if (data.isValid())
            return data.toInt();

        return 0;
    }

    [[nodiscard]] std::optional<GridPreviewWindow::SeedDefinition> definition() const
    {
        if (!hasValidTargetColor() || !hasValidWeight())
            return std::nullopt;

        GridPreviewWindow::SeedDefinition result;
        result.targetColor = targetColorValue();
        bool weightOk = false;
        result.weight = weightValue(&weightOk);
        if (!weightOk)
            return std::nullopt;

        result.channel = channelIsEmpty() ? 0 : channelValue();
        if (!channelIsEmpty()) {
            bool seedOk = false;
            result.seedColor = seedColorValue(&seedOk);
            if (!seedOk)
                return std::nullopt;
            result.seedColor.setAlpha(255);
        } else {
            result.seedColor = QColor(0, 0, 0, 0);
        }

        return result;
    }

    [[nodiscard]] QLineEdit *seedColorLineEdit() const { return m_seedColorEdit; }
    [[nodiscard]] QLineEdit *targetColorLineEdit() const { return m_targetColorEdit; }
    [[nodiscard]] QLineEdit *targetWeightLineEdit() const { return m_targetWeightEdit; }
    [[nodiscard]] QComboBox *channelComboBox() const { return m_channelCombo; }
    [[nodiscard]] ColorPreviewLabel *seedColorPreviewLabel() const { return m_seedColorPreview; }
    [[nodiscard]] ColorPreviewLabel *targetColorPreviewLabel() const { return m_targetColorPreview; }

private:
    void setupUi()
    {
        auto *mainLayout = new QGridLayout(this);
        mainLayout->setContentsMargins(6, 6, 18, 6);
        mainLayout->setHorizontalSpacing(8);
        mainLayout->setVerticalSpacing(4);
        mainLayout->setColumnStretch(1, 1);
        mainLayout->setColumnStretch(2, 1);

        m_numberLabel = new QLabel(this);
        m_numberLabel->setMinimumWidth(24);
        m_numberLabel->setAlignment(Qt::AlignCenter);

        auto *seedColorLabel = new QLabel(QStringLiteral("Seed color"), this);
        m_seedColorEdit = new QLineEdit(this);
        m_seedColorEdit->setPlaceholderText(QStringLiteral("255,255,255"));
        m_seedColorEdit->setText(QStringLiteral("255,255,255"));
        m_seedColorPreview = new ColorPreviewLabel(this);
        m_seedColorPreview->setFixedSize(32, 20);
        m_seedColorPreview->setFrameShape(QFrame::Box);

        auto *channelLabel = new QLabel(QStringLiteral("Channel"), this);
        m_channelCombo = new QComboBox(this);
        m_channelCombo->addItem(QStringLiteral("empty"));
        for (int i = 1; i <= kChannelCount; ++i)
            m_channelCombo->addItem(QString::number(i), QVariant(i));

        auto *targetColorLabel = new QLabel(QStringLiteral("Target color"), this);
        m_targetColorEdit = new QLineEdit(this);
        m_targetColorEdit->setPlaceholderText(QStringLiteral("255,255,255"));
        m_targetColorEdit->setText(QStringLiteral("255,255,255"));
        m_targetColorPreview = new ColorPreviewLabel(this);
        m_targetColorPreview->setFixedSize(32, 20);
        m_targetColorPreview->setFrameShape(QFrame::Box);

        auto *targetWeightLabel = new QLabel(QStringLiteral("Target weight"), this);
        m_targetWeightEdit = new QLineEdit(this);
        m_targetWeightEdit->setPlaceholderText(QStringLiteral("1.0"));
        m_targetWeightEdit->setText(QStringLiteral("1.0"));

        mainLayout->addWidget(new QLabel(QStringLiteral("#"), this), 0, 0);
        mainLayout->addWidget(m_numberLabel, 0, 1);
        mainLayout->addWidget(seedColorLabel, 1, 0, 1, 1);
        mainLayout->addWidget(m_seedColorEdit, 1, 1, 1, 2);
        mainLayout->addWidget(m_seedColorPreview, 1, 3);
        mainLayout->addWidget(channelLabel, 2, 0);
        mainLayout->addWidget(m_channelCombo, 2, 1, 1, 3);
        mainLayout->addWidget(targetColorLabel, 3, 0);
        mainLayout->addWidget(m_targetColorEdit, 3, 1, 1, 2);
        mainLayout->addWidget(m_targetColorPreview, 3, 3);
        mainLayout->addWidget(targetWeightLabel, 4, 0);
        mainLayout->addWidget(m_targetWeightEdit, 4, 1, 1, 3);

        connect(m_seedColorEdit, &QLineEdit::textChanged, this, [this]() {
            updateColorPreview(m_seedColorEdit->text(), m_seedColorPreview);
        });
        connect(m_targetColorEdit, &QLineEdit::textChanged, this, [this]() {
            updateColorPreview(m_targetColorEdit->text(), m_targetColorPreview);
        });
        connect(m_channelCombo, &QComboBox::currentIndexChanged, this, [this]() {
            updateSeedColorState();
        });

        updateColorPreview(m_seedColorEdit->text(), m_seedColorPreview);
        updateColorPreview(m_targetColorEdit->text(), m_targetColorPreview);
        updateSeedColorState();
    }

    static QColor parseColorString(const QString &text, bool *ok = nullptr)
    {
        if (ok)
            *ok = false;

        const QString trimmed = text.trimmed();
        if (trimmed.isEmpty())
            return {};

        const QStringList parts = trimmed.split(',', Qt::SkipEmptyParts);
        if (parts.size() != 3)
            return {};

        int rgb[3] = {0, 0, 0};
        for (int i = 0; i < 3; ++i) {
            bool componentOk = false;
            const int value = parts[i].trimmed().toInt(&componentOk);
            if (!componentOk || value < 0 || value > 255)
                return {};
            rgb[i] = value;
        }

        if (ok)
            *ok = true;
        return QColor(rgb[0], rgb[1], rgb[2]);
    }

    static void updateColorPreview(const QString &text, QLabel *preview)
    {
        if (!preview)
            return;

        bool ok = false;
        const QColor color = parseColorString(text, &ok);
        const QSize previewSize = preview->size();
        if (!previewSize.isValid())
            return;

        QPixmap pixmap(previewSize);
        if (ok) {
            pixmap.fill(color);
        } else {
            pixmap.fill(Qt::white);
            QPainter painter(&pixmap);
            QPen pen(Qt::black);
            pen.setWidth(2);
            painter.setPen(pen);
            painter.drawLine(QPointF(0.0, previewSize.height()), QPointF(previewSize.width(), 0.0));
            painter.end();
        }

        preview->setPixmap(pixmap);
        preview->setToolTip(ok ? color.name() : QStringLiteral("Enter RGB as R,G,B"));
    }

    static void setDiagonalPreview(QLabel *preview)
    {
        if (!preview)
            return;

        const QSize previewSize = preview->size();
        if (!previewSize.isValid())
            return;

        QPixmap pixmap(previewSize);
        pixmap.fill(Qt::white);

        QPainter painter(&pixmap);
        QPen pen(Qt::black);
        pen.setWidth(2);
        painter.setPen(pen);
        painter.drawLine(QPointF(0.0, previewSize.height()), QPointF(previewSize.width(), 0.0));
        painter.end();

        preview->setPixmap(pixmap);
        preview->setToolTip(QString());
    }

    void updateSeedColorState()
    {
        const bool channelIsEmpty = m_channelCombo && m_channelCombo->currentIndex() == 0;

        if (m_seedColorEdit)
            m_seedColorEdit->setEnabled(!channelIsEmpty);

        if (!m_seedColorPreview)
            return;

        if (channelIsEmpty)
            setDiagonalPreview(m_seedColorPreview);
        else
            updateColorPreview(m_seedColorEdit ? m_seedColorEdit->text() : QString(), m_seedColorPreview);
    }

    QLabel *m_numberLabel = nullptr;
    QLineEdit *m_seedColorEdit = nullptr;
    ColorPreviewLabel *m_seedColorPreview = nullptr;
    QComboBox *m_channelCombo = nullptr;
    QLineEdit *m_targetColorEdit = nullptr;
    ColorPreviewLabel *m_targetColorPreview = nullptr;
    QLineEdit *m_targetWeightEdit = nullptr;
};
}

GridPreviewWindow::GridPreviewWindow(QWidget *parent)
    : QDialog(parent)
    , m_imageLabel(new QLabel(this))
    , m_scrollArea(new QScrollArea(this))
    , m_seeEffectButton(new QPushButton(tr("Show effect"), this))
    , m_toggleGridLinesButton(new QPushButton(tr("Hide grid lines"), this))
    , m_commitButton(new QPushButton(tr("Commit changes"), this))
    , m_addSeedButton(new QPushButton(tr("Add seed"), this))
    , m_deleteSeedButton(new QPushButton(tr("Delete seed"), this))
    , m_applyChangesButton(new QPushButton(tr("Apply changes"), this))
    , m_seedListWidget(new QListWidget(this))
{
    setWindowTitle(tr("Pinned Image Grid"));
    setModal(false);

    m_scrollArea->setWidgetResizable(true);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_scrollArea->setWidget(m_imageLabel);

    m_seedListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_seedListWidget->setSpacing(4);
    m_seedListWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_seedListWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *mainLayout = new QVBoxLayout(this);
    auto *contentLayout = new QHBoxLayout();
    auto *sideLayout = new QVBoxLayout();
    auto *seedButtonLayout = new QHBoxLayout();

    sideLayout->addWidget(m_seeEffectButton);
    sideLayout->addWidget(m_toggleGridLinesButton);
    seedButtonLayout->addWidget(m_addSeedButton);
    seedButtonLayout->addWidget(m_deleteSeedButton);
    sideLayout->addLayout(seedButtonLayout);
    sideLayout->addWidget(m_seedListWidget);
    sideLayout->addWidget(m_applyChangesButton);
    sideLayout->setStretch(3, 1);

    contentLayout->addWidget(m_scrollArea, 2);
    contentLayout->addLayout(sideLayout, 1);

    auto *bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch(1);
    bottomLayout->addWidget(m_commitButton);

    mainLayout->addLayout(contentLayout);
    mainLayout->addLayout(bottomLayout);

    m_paletteContainer = new QFrame(this);
    m_paletteContainer->setVisible(false);
    m_paletteContainer->setFrameShape(QFrame::StyledPanel);
    m_paletteContainer->setAutoFillBackground(true);
    m_paletteContainer->setAttribute(Qt::WA_StyledBackground, true);

    m_paletteLayout = new QVBoxLayout(m_paletteContainer);
    m_paletteLayout->setContentsMargins(8, 8, 8, 8);
    m_paletteLayout->setSpacing(6);

    auto *paletteButtonRow = new QHBoxLayout();
    paletteButtonRow->setContentsMargins(0, 0, 0, 0);
    paletteButtonRow->setSpacing(6);

    m_showPaletteButton = new QPushButton(tr("Show color palette"), m_paletteContainer);
    m_exitColorSelectionButton = new QPushButton(tr("Exit color-selection mode"), m_paletteContainer);

    paletteButtonRow->addWidget(m_showPaletteButton);
    paletteButtonRow->addWidget(m_exitColorSelectionButton);

    m_paletteImageLabel = new QLabel(m_paletteContainer);
    m_paletteImageLabel->setVisible(false);
    m_paletteImageLabel->setFrameShape(QFrame::Box);
    m_paletteImageLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_paletteImageLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_paletteLayout->addLayout(paletteButtonRow);
    m_paletteLayout->addWidget(m_paletteImageLabel);

    connect(m_seeEffectButton, &QPushButton::clicked, this, &GridPreviewWindow::handleSeeEffectClicked);
    connect(m_toggleGridLinesButton, &QPushButton::clicked, this, &GridPreviewWindow::handleToggleGridLinesClicked);
    connect(m_commitButton, &QPushButton::clicked, this, &GridPreviewWindow::handleCommitClicked);
    connect(m_addSeedButton, &QPushButton::clicked, this, &GridPreviewWindow::handleAddSeedClicked);
    connect(m_deleteSeedButton, &QPushButton::clicked, this, &GridPreviewWindow::handleDeleteSeedClicked);
    connect(m_applyChangesButton, &QPushButton::clicked, this, &GridPreviewWindow::handleApplyChangesClicked);
    connect(m_showPaletteButton, &QPushButton::clicked, this, &GridPreviewWindow::handlePaletteButtonClicked);
    connect(m_exitColorSelectionButton, &QPushButton::clicked, this, [this]() {
        stopColorPicking();
    });
    connect(m_seedListWidget, &QListWidget::currentRowChanged, this, [this]() {
        updateButtonStates();
    });

    updateButtonStates();
    updateColorSelectionUiState();

    resize(900, 650);
}

GridPreviewWindow::~GridPreviewWindow()
{
    stopColorPicking();
}

void GridPreviewWindow::setImageWithGrid(const QPixmap &pixmap, double widthMeters, double heightMeters)
{
    stopColorPicking();

    m_originalPixmap = pixmap;
    m_originalWithGridPixmap = {};
    m_effectPixmap = {};
    m_effectWithGridPixmap = {};
    m_effectPreviewPixmap = {};
    m_effectPreviewWithGridPixmap = {};
    m_originalGridImage = {};
    m_modifiedGridImage = {};
    m_appliedSeedChannels.clear();
    m_gridColumns = 0;
    m_gridRows = 0;
    m_showEffect = true;
    m_showGridLines = true;
    m_highlightEmptyCells = false;
    m_cellWidthPx = 0;
    m_cellHeightPx = 0;

    if (m_seedListWidget)
        m_seedListWidget->clear();

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

    if (!ensureOriginalGridImage() || !rebuildEffectPixmapsFromModifiedGrid())
        m_showEffect = false;

    updateDisplayedPixmap();
}

void GridPreviewWindow::updateDisplayedPixmap()
{
    QPixmap displayPixmap;

    if (m_showEffect) {
        if (!ensureEffectPixmaps())
            m_showEffect = false;
        else if (m_showGridLines)
            displayPixmap = m_effectPreviewWithGridPixmap.isNull() ? m_effectWithGridPixmap : m_effectPreviewWithGridPixmap;
        else
            displayPixmap = m_effectPreviewPixmap.isNull() ? m_effectPixmap : m_effectPreviewPixmap;
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
    if (!m_effectPixmap.isNull() && !m_effectWithGridPixmap.isNull())
        return true;

    if (!ensureOriginalGridImage())
        return false;

    return rebuildEffectPixmapsFromModifiedGrid();
}

bool GridPreviewWindow::ensureOriginalGridImage()
{
    if (!m_originalGridImage.isNull() && m_gridColumns > 0 && m_gridRows > 0)
        return true;

    if (m_originalPixmap.isNull() || m_cellWidthPx <= 0 || m_cellHeightPx <= 0)
        return false;

    m_gridColumns = m_originalPixmap.width() / m_cellWidthPx;
    m_gridRows = m_originalPixmap.height() / m_cellHeightPx;

    if (m_gridColumns <= 0 || m_gridRows <= 0)
        return false;

    const int cropWidth = m_gridColumns * m_cellWidthPx;
    const int cropHeight = m_gridRows * m_cellHeightPx;

    QImage sourceImage = m_originalPixmap.toImage().convertToFormat(QImage::Format_ARGB32);
    if (sourceImage.isNull() || sourceImage.width() < cropWidth || sourceImage.height() < cropHeight)
        return false;

    QImage cropped = sourceImage.copy(0, 0, cropWidth, cropHeight);
    if (cropped.isNull())
        return false;

    QImage reduced = cropped.scaled(m_gridColumns, m_gridRows, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    if (reduced.isNull())
        return false;

    if (reduced.format() != QImage::Format_ARGB32)
        reduced = reduced.convertToFormat(QImage::Format_ARGB32);

    m_originalGridImage = reduced;
    m_modifiedGridImage = reduced;
    m_appliedSeedChannels = QVector<QVector<int>>(m_gridRows, QVector<int>(m_gridColumns, 0));

    return true;
}

bool GridPreviewWindow::rebuildEffectPixmapsFromModifiedGrid()
{
    if (m_modifiedGridImage.isNull() || m_cellWidthPx <= 0 || m_cellHeightPx <= 0 || m_gridColumns <= 0 || m_gridRows <= 0)
        return false;

    const int cropWidth = m_gridColumns * m_cellWidthPx;
    const int cropHeight = m_gridRows * m_cellHeightPx;

    QImage effectImage = m_modifiedGridImage.scaled(cropWidth, cropHeight, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    if (effectImage.isNull())
        return false;

    if (effectImage.format() != QImage::Format_ARGB32)
        effectImage = effectImage.convertToFormat(QImage::Format_ARGB32);

    m_effectPixmap = QPixmap::fromImage(effectImage);
    m_effectWithGridPixmap = drawGridLines(m_effectPixmap);

    QImage previewImage = effectImage;
    if (m_highlightEmptyCells)
        applyEmptyChannelHighlight(previewImage);

    m_effectPreviewPixmap = QPixmap::fromImage(previewImage);
    m_effectPreviewWithGridPixmap = drawGridLines(m_effectPreviewPixmap);

    if (m_effectPreviewPixmap.isNull())
        m_effectPreviewPixmap = m_effectPixmap;
    if (m_effectPreviewWithGridPixmap.isNull())
        m_effectPreviewWithGridPixmap = m_effectWithGridPixmap;

    return !m_effectPixmap.isNull();
}

void GridPreviewWindow::applyEmptyChannelHighlight(QImage &image) const
{
    if (!m_highlightEmptyCells || image.isNull())
        return;

    if (m_cellWidthPx <= 0 || m_cellHeightPx <= 0)
        return;

    if (m_appliedSeedChannels.isEmpty())
        return;

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, false);

    const QColor fillColor(144, 238, 144, 96);
    const QColor edgeColor(34, 139, 34, 255);
    const QColor gridColor(34, 139, 34, 180);

    for (int row = 0; row < m_appliedSeedChannels.size(); ++row) {
        if (row < 0 || row >= m_gridRows)
            continue;

        const QVector<int> &rowValues = m_appliedSeedChannels.at(row);
        for (int column = 0; column < rowValues.size(); ++column) {
            if (rowValues.at(column) != 0)
                continue;

            const QRect cellRect(column * m_cellWidthPx, row * m_cellHeightPx, m_cellWidthPx, m_cellHeightPx);
            painter.fillRect(cellRect, fillColor);

            QPen borderPen(edgeColor);
            borderPen.setWidth(2);
            borderPen.setJoinStyle(Qt::MiterJoin);
            painter.setPen(borderPen);
            painter.drawRect(cellRect.adjusted(0, 0, -1, -1));

            QPen gridPen(gridColor);
            gridPen.setWidth(1);
            gridPen.setCapStyle(Qt::SquareCap);
            gridPen.setJoinStyle(Qt::MiterJoin);
            painter.setPen(gridPen);

            const int step = std::max(4, std::min(m_cellWidthPx, m_cellHeightPx) / 4);
            for (int x = cellRect.left() + step; x < cellRect.right(); x += step)
                painter.drawLine(QPoint(x, cellRect.top()), QPoint(x, cellRect.bottom() - 1));
            for (int y = cellRect.top() + step; y < cellRect.bottom(); y += step)
                painter.drawLine(QPoint(cellRect.left(), y), QPoint(cellRect.right() - 1, y));
        }
    }
}

void GridPreviewWindow::updateButtonStates()
{
    const bool hasOriginal = !m_originalPixmap.isNull();
    const bool effectPossible = hasOriginal && m_cellWidthPx > 0 && m_cellHeightPx > 0 &&
                                 (m_originalPixmap.width() / m_cellWidthPx) > 0 &&
                                 (m_originalPixmap.height() / m_cellHeightPx) > 0;

    if (m_seeEffectButton) {
        m_seeEffectButton->setEnabled(effectPossible || m_showEffect);
        m_seeEffectButton->setText(m_showEffect ? tr("Show original") : tr("Show effect"));
    }

    if (m_toggleGridLinesButton) {
        const bool hasPixmap = (m_showEffect && !m_effectPixmap.isNull()) || (!m_showEffect && hasOriginal);
        m_toggleGridLinesButton->setEnabled(hasPixmap);
        m_toggleGridLinesButton->setText(m_showGridLines ? tr("Hide grid lines") : tr("Show grid lines"));
    }

    if (m_commitButton)
        m_commitButton->setEnabled(effectPossible);

    if (m_addSeedButton)
        m_addSeedButton->setEnabled(true);

    if (m_deleteSeedButton) {
        const bool hasSelection = m_seedListWidget && m_seedListWidget->currentRow() >= 0;
        m_deleteSeedButton->setEnabled(hasSelection);
    }

    if (m_applyChangesButton)
        m_applyChangesButton->setEnabled(allSeedInputsValid());
}

void GridPreviewWindow::updateSeedItemNumbers()
{
    if (!m_seedListWidget)
        return;

    for (int i = 0; i < m_seedListWidget->count(); ++i) {
        QListWidgetItem *item = m_seedListWidget->item(i);
        if (!item)
            continue;

        QWidget *widget = m_seedListWidget->itemWidget(item);
        if (auto *seedWidget = dynamic_cast<SeedItemWidget *>(widget))
            seedWidget->setItemNumber(i + 1);
    }

    updateButtonStates();
}

bool GridPreviewWindow::allSeedInputsValid() const
{
    if (!m_seedListWidget || m_seedListWidget->count() < 2)
        return false;

    for (int i = 0; i < m_seedListWidget->count(); ++i) {
        QListWidgetItem *item = m_seedListWidget->item(i);
        if (!item)
            return false;

        QWidget *widget = m_seedListWidget->itemWidget(item);
        auto *seedWidget = dynamic_cast<SeedItemWidget *>(widget);
        if (!seedWidget)
            return false;

        if (!seedWidget->hasValidTargetColor() || !seedWidget->hasValidWeight())
            return false;

        if (!seedWidget->channelIsEmpty() && !seedWidget->hasValidSeedColor())
            return false;
    }

    return true;
}

bool GridPreviewWindow::collectSeedDefinitions(QVector<SeedDefinition> &outSeeds) const
{
    outSeeds.clear();

    if (!m_seedListWidget || m_seedListWidget->count() == 0)
        return false;

    outSeeds.reserve(m_seedListWidget->count());

    for (int i = 0; i < m_seedListWidget->count(); ++i) {
        QListWidgetItem *item = m_seedListWidget->item(i);
        if (!item)
            return false;

        QWidget *widget = m_seedListWidget->itemWidget(item);
        auto *seedWidget = dynamic_cast<SeedItemWidget *>(widget);
        if (!seedWidget)
            return false;

        auto definition = seedWidget->definition();
        if (!definition)
            return false;

        outSeeds.append(*definition);
    }

    return !outSeeds.isEmpty();
}

void GridPreviewWindow::connectSeedWidgetSignals(QWidget *widget)
{
    auto *seedWidget = dynamic_cast<SeedItemWidget *>(widget);
    if (!seedWidget)
        return;

    if (auto *edit = seedWidget->seedColorLineEdit())
        connect(edit, &QLineEdit::textChanged, this, [this]() {
            updateButtonStates();
        });

    if (auto *edit = seedWidget->targetColorLineEdit())
        connect(edit, &QLineEdit::textChanged, this, [this]() {
            updateButtonStates();
        });

    if (auto *edit = seedWidget->targetWeightLineEdit())
        connect(edit, &QLineEdit::textChanged, this, [this]() {
            updateButtonStates();
        });

    if (auto *combo = seedWidget->channelComboBox())
        connect(combo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this, seedWidget](int) {
            if (m_activeColorPreview == seedWidget->seedColorPreviewLabel())
                stopColorPicking();
            updateButtonStates();
        });

    if (auto *preview = seedWidget->seedColorPreviewLabel())
        preview->setClickCallback([this, seedWidget]() {
            startColorPickingForWidget(seedWidget->seedColorLineEdit(), seedWidget->seedColorPreviewLabel());
        });

    if (auto *preview = seedWidget->targetColorPreviewLabel())
        preview->setClickCallback([this, seedWidget]() {
            startColorPickingForWidget(seedWidget->targetColorLineEdit(), seedWidget->targetColorPreviewLabel());
        });
}

void GridPreviewWindow::startColorPickingForWidget(QLineEdit *lineEdit, QLabel *previewLabel)
{
    if (!lineEdit || !previewLabel)
        return;

    if (!lineEdit->isEnabled()) {
        if (m_activeColorPreview == previewLabel)
            stopColorPicking();
        return;
    }

    if (m_activeColorPreview == previewLabel) {
        stopColorPicking();
        return;
    }

    stopColorPicking();

    if (!m_colorPickerOverlay)
        m_colorPickerOverlay = std::make_unique<ColorPickerOverlay>(this);

    m_activeColorLineEdit = lineEdit;
    m_activeColorPreview = previewLabel;
    m_activePreviewOriginalStyle = previewLabel->styleSheet();
    previewLabel->setStyleSheet(QStringLiteral("border: 2px solid #1e7f1e;"));

    m_paletteVisible = false;
    if (m_paletteImageLabel)
        m_paletteImageLabel->setVisible(false);

    updateColorSelectionUiState();
    if (m_showPaletteButton)
        m_showPaletteButton->setText(tr("Show color palette"));

    m_colorPickerOverlay->setCanceledCallback([this]() {
        stopColorPicking();
    });

    m_colorPickerOverlay->setColorPickedCallback([this](const QColor &color) {
        if (m_activeColorLineEdit && color.isValid()) {
            const QString text = QStringLiteral("%1,%2,%3").arg(color.red()).arg(color.green()).arg(color.blue());
            m_activeColorLineEdit->setText(text);
        }
    });

    m_colorPickerOverlay->setOriginWidget(previewLabel);
    m_colorPickerOverlay->setPaletteButton(m_showPaletteButton);
    m_colorPickerOverlay->setExitButton(m_exitColorSelectionButton);

    m_colorPickerOverlay->begin();
    updatePalettePanelGeometry();
}

void GridPreviewWindow::stopColorPicking()
{
    if (m_colorPickerOverlay) {
        m_colorPickerOverlay->setColorPickedCallback({});
        m_colorPickerOverlay->setCanceledCallback({});
        m_colorPickerOverlay->setOriginWidget(nullptr);
        m_colorPickerOverlay->setPaletteButton(nullptr);
        m_colorPickerOverlay->setExitButton(nullptr);
        m_colorPickerOverlay->finish();
    }

    if (m_activeColorPreview)
        m_activeColorPreview->setStyleSheet(m_activePreviewOriginalStyle);

    m_activePreviewOriginalStyle.clear();
    m_activeColorLineEdit = nullptr;
    m_activeColorPreview = nullptr;

    m_paletteVisible = false;
    if (m_paletteImageLabel)
        m_paletteImageLabel->setVisible(false);
    if (m_paletteContainer)
        m_paletteContainer->hide();
    updateColorSelectionUiState();
}

void GridPreviewWindow::updateColorSelectionUiState()
{
    const bool picking = m_activeColorPreview != nullptr;

    if (m_paletteContainer) {
        m_paletteContainer->setVisible(picking);
        if (picking)
            updatePalettePanelGeometry();
    }

    if (m_showPaletteButton) {
        if (!picking)
            m_showPaletteButton->setText(tr("Show color palette"));
        m_showPaletteButton->setEnabled(picking);
    }

    if (m_exitColorSelectionButton)
        m_exitColorSelectionButton->setEnabled(picking);

    if (!picking) {
        m_paletteVisible = false;
        if (m_paletteImageLabel)
            m_paletteImageLabel->setVisible(false);
    }
}

void GridPreviewWindow::updatePalettePanelGeometry()
{
    if (!m_paletteContainer || !m_paletteContainer->isVisible())
        return;

    m_paletteContainer->adjustSize();
    const QSize hint = m_paletteContainer->sizeHint();
    if (hint.isValid())
        m_paletteContainer->resize(hint);

    const QMargins margins = layout() ? layout()->contentsMargins() : QMargins();
    const int offset = 8;
    int x = margins.left() + offset;

    int bottomY = height() - margins.bottom();
    if (m_commitButton) {
        const QPoint bottomLeft = m_commitButton->mapTo(this, QPoint(0, m_commitButton->height()));
        bottomY = bottomLeft.y();
    }

    int y = bottomY - m_paletteContainer->height() - offset;
    if (y < margins.top() + offset)
        y = margins.top() + offset;

    m_paletteContainer->move(x, y);
    m_paletteContainer->raise();
}

void GridPreviewWindow::handlePaletteButtonClicked()
{
    if (!m_paletteImageLabel || !m_showPaletteButton)
        return;

    m_paletteVisible = !m_paletteVisible;

    if (m_paletteVisible) {
        QPixmap palettePixmap(QStringLiteral(":/color_gradient_720p_extreme_vibrant.png"));
        if (!palettePixmap.isNull()) {
            const int maxWidth = 260;
            const QSize targetSize(maxWidth, static_cast<int>(std::round(maxWidth * palettePixmap.height() / static_cast<double>(palettePixmap.width()))));
            m_paletteImageLabel->setPixmap(palettePixmap.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            m_paletteImageLabel->adjustSize();
        } else {
            m_paletteImageLabel->setPixmap(QPixmap());
        }
    }

    if (m_paletteImageLabel)
        m_paletteImageLabel->setVisible(m_paletteVisible);

    m_showPaletteButton->setText(m_paletteVisible ? tr("Hide color palette") : tr("Show color palette"));
    updatePalettePanelGeometry();
}

void GridPreviewWindow::handleAddSeedClicked()
{
    if (!m_seedListWidget)
        return;

    auto *item = new QListWidgetItem();
    auto *widget = new SeedItemWidget(m_seedListWidget->count() + 1, m_seedListWidget);
    item->setSizeHint(widget->sizeHint());
    m_seedListWidget->addItem(item);
    m_seedListWidget->setItemWidget(item, widget);
    m_seedListWidget->setCurrentItem(item);

    connectSeedWidgetSignals(widget);

    updateButtonStates();
}

void GridPreviewWindow::handleDeleteSeedClicked()
{
    if (!m_seedListWidget)
        return;

    const int currentRow = m_seedListWidget->currentRow();
    if (currentRow < 0)
        return;

    QListWidgetItem *item = m_seedListWidget->takeItem(currentRow);
    delete item;

    updateSeedItemNumbers();
}

void GridPreviewWindow::handleApplyChangesClicked()
{
    if (!allSeedInputsValid()) {
        QMessageBox::warning(this, tr("Apply changes"), tr("Please provide at least two valid seeds with colors and weights."));
        return;
    }

    if (!ensureOriginalGridImage()) {
        QMessageBox::warning(this, tr("Apply changes"), tr("Unable to prepare the image grid for modification."));
        return;
    }

    QVector<SeedDefinition> seeds;
    if (!collectSeedDefinitions(seeds)) {
        QMessageBox::warning(this, tr("Apply changes"), tr("Some seed values are invalid. Please correct them and try again."));
        return;
    }

    if (seeds.isEmpty())
        return;

    if (m_gridColumns <= 0 || m_gridRows <= 0 || m_originalGridImage.isNull())
        return;

    QImage newGrid(m_gridColumns, m_gridRows, QImage::Format_ARGB32);
    newGrid.fill(Qt::transparent);

    QVector<QVector<int>> channelGrid(m_gridRows, QVector<int>(m_gridColumns, 0));

    for (int y = 0; y < m_gridRows; ++y) {
        for (int x = 0; x < m_gridColumns; ++x) {
            const QColor oriColor = QColor::fromRgba(m_originalGridImage.pixel(x, y));

            double bestValue = std::numeric_limits<double>::infinity();
            const SeedDefinition *bestSeed = nullptr;

            for (const SeedDefinition &seed : seeds) {
                if (seed.weight <= 0.0)
                    continue;

                const double dr = static_cast<double>(oriColor.red()) - static_cast<double>(seed.targetColor.red());
                const double dg = static_cast<double>(oriColor.green()) - static_cast<double>(seed.targetColor.green());
                const double db = static_cast<double>(oriColor.blue()) - static_cast<double>(seed.targetColor.blue());
                const double diff = dr * dr + dg * dg + db * db;
                const double value = diff / seed.weight;

                if (value < bestValue) {
                    bestValue = value;
                    bestSeed = &seed;
                }
            }

            if (!bestSeed)
                continue;

            if (bestSeed->channelIsEmpty()) {
                newGrid.setPixelColor(x, y, QColor(0, 0, 0, 0));
                channelGrid[y][x] = 0;
            } else {
                QColor seedColor = bestSeed->seedColor;
                seedColor.setAlpha(255);
                newGrid.setPixelColor(x, y, seedColor);
                channelGrid[y][x] = bestSeed->channel;
            }
        }
    }

    m_modifiedGridImage = newGrid;
    m_appliedSeedChannels = channelGrid;
    m_effectPixmap = {};
    m_effectWithGridPixmap = {};
    m_effectPreviewPixmap = {};
    m_effectPreviewWithGridPixmap = {};
    m_highlightEmptyCells = true;

    if (!rebuildEffectPixmapsFromModifiedGrid()) {
        QMessageBox::warning(this, tr("Apply changes"), tr("Unable to generate the modified image."));
        return;
    }

    m_showEffect = true;
    updateDisplayedPixmap();
}

void GridPreviewWindow::handleSeeEffectClicked()
{
    m_showEffect = !m_showEffect;
    if (m_showEffect && !ensureEffectPixmaps())
        m_showEffect = false;
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

    emit effectCommitted(m_effectPixmap, m_appliedSeedChannels);
    close();
}

void GridPreviewWindow::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    updatePalettePanelGeometry();
}

