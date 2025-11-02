#include "CameraStreamWindow.h"

#include <QCloseEvent>
#include <QImage>
#include <QLabel>
#include <QNetworkReply>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QNetworkRequest>
#include <QSizePolicy>
#include <QtGlobal>

CameraStreamWindow::CameraStreamWindow(const QUrl &streamUrl, QWidget *parent)
    : QWidget(parent, Qt::Dialog)
    , m_streamUrl(streamUrl)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    setWindowTitle(tr("Camera Monitor"));
    resize(640, 480);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_videoLabel = new QLabel(tr("Connecting to camera..."), this);
    m_videoLabel->setAlignment(Qt::AlignCenter);
    m_videoLabel->setMinimumSize(320, 240);
    m_videoLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(m_videoLabel);

    startStream();
}

CameraStreamWindow::~CameraStreamWindow()
{
    stopStream();
}

void CameraStreamWindow::startStream()
{
    stopStream();

    if (!m_streamUrl.isValid()) {
        resetPlaceholder(tr("Invalid camera stream URL."));
        emit statusMessageRequested(tr("Camera stream URL is invalid."), 5000);
        return;
    }

    QNetworkRequest request(m_streamUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("TestMapWidget/1.0"));

    m_streamReply = m_networkManager.get(request);
    if (!m_streamReply) {
        resetPlaceholder(tr("Unable to open camera stream."));
        emit statusMessageRequested(tr("Unable to connect to camera stream."), 5000);
        return;
    }

    connect(m_streamReply, &QNetworkReply::readyRead, this, &CameraStreamWindow::handleReadyRead);
    connect(m_streamReply, &QNetworkReply::errorOccurred, this, &CameraStreamWindow::handleError);
    connect(m_streamReply, &QNetworkReply::finished, this, &CameraStreamWindow::handleFinished);

    emit statusMessageRequested(tr("Connecting to camera stream..."), 3000);
}

void CameraStreamWindow::stopStream()
{
    if (!m_streamReply)
        return;

    disconnect(m_streamReply, nullptr, this, nullptr);
    m_streamReply->abort();
    m_streamReply->deleteLater();
    m_streamReply = nullptr;
    m_buffer.clear();
    m_lastFrame = QPixmap();
    m_receivedFirstFrame = false;
}

void CameraStreamWindow::handleReadyRead()
{
    if (!m_streamReply)
        return;

    m_buffer.append(m_streamReply->readAll());

    while (true) {
        const int startIndex = m_buffer.indexOf("\xFF\xD8");
        if (startIndex < 0) {
            const qsizetype maxBufferSize = 512 * 1024;
            if (m_buffer.size() > maxBufferSize)
                m_buffer.remove(0, m_buffer.size() - 2);
            break;
        }

        if (startIndex > 0)
            m_buffer.remove(0, startIndex);

        const int endIndex = m_buffer.indexOf("\xFF\xD9", 2);
        if (endIndex < 0)
            break;

        QByteArray frameData = m_buffer.left(endIndex + 2);
        m_buffer.remove(0, endIndex + 2);

        QImage frameImage = QImage::fromData(frameData, "JPG");
        if (frameImage.isNull())
            continue;

        m_lastFrame = QPixmap::fromImage(frameImage);
        if (m_videoLabel)
            m_videoLabel->setText(QString());
        updateDisplayedPixmap();

        if (!m_receivedFirstFrame) {
            m_receivedFirstFrame = true;
            emit statusMessageRequested(tr("Camera stream active."), 3000);
        }
    }
}

void CameraStreamWindow::handleError(QNetworkReply::NetworkError)
{
    if (!m_streamReply)
        return;

    const QString errorText = tr("Camera stream error: %1").arg(m_streamReply->errorString());
    emit statusMessageRequested(errorText, 5000);
    resetPlaceholder(errorText);
}

void CameraStreamWindow::handleFinished()
{
    if (!m_streamReply)
        return;

    if (m_streamReply->error() == QNetworkReply::NoError) {
        resetPlaceholder(tr("Camera stream ended."));
        emit statusMessageRequested(tr("Camera stream ended."), 3000);
    }

    stopStream();
}

void CameraStreamWindow::closeEvent(QCloseEvent *event)
{
    stopStream();
    emit windowClosed();
    QWidget::closeEvent(event);
}

void CameraStreamWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateDisplayedPixmap();
}

void CameraStreamWindow::updateDisplayedPixmap()
{
    if (!m_videoLabel)
        return;

    if (m_lastFrame.isNull()) {
        m_videoLabel->setPixmap(QPixmap());
        return;
    }

    const QSize labelSize = m_videoLabel->size();
    if (labelSize.isEmpty()) {
        m_videoLabel->setPixmap(m_lastFrame);
        return;
    }

    const QPixmap scaledPixmap = m_lastFrame.scaled(labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_videoLabel->setPixmap(scaledPixmap);
}

void CameraStreamWindow::resetPlaceholder(const QString &text)
{
    if (!m_videoLabel)
        return;

    m_videoLabel->setPixmap(QPixmap());
    m_videoLabel->setText(text);
}
