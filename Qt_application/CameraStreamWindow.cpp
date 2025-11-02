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

// CameraStreamWorker implementation

CameraStreamWorker::CameraStreamWorker(QObject *parent)
    : QObject(parent)
{
}

void CameraStreamWorker::startStream(const QUrl &streamUrl)
{
    stopStream();

    m_streamUrl = streamUrl;

    if (!m_streamUrl.isValid()) {
        const QString message = tr("Camera stream URL is invalid.");
        emit errorOccurred(message);
        emit statusMessageRequested(message, 5000);
        emit streamStopped(message);
        return;
    }

    if (!m_networkManager)
        m_networkManager = new QNetworkAccessManager(this);

    QNetworkRequest request(m_streamUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("TestMapWidget/1.0"));

    m_streamReply = m_networkManager->get(request);
    if (!m_streamReply) {
        const QString message = tr("Unable to connect to camera stream.");
        emit errorOccurred(message);
        emit statusMessageRequested(message, 5000);
        emit streamStopped(message);
        return;
    }

    connect(m_streamReply, &QNetworkReply::readyRead, this, &CameraStreamWorker::handleReadyRead);
    connect(m_streamReply, &QNetworkReply::errorOccurred, this, &CameraStreamWorker::handleError);
    connect(m_streamReply, &QNetworkReply::finished, this, &CameraStreamWorker::handleFinished);

    resetState();
    emit statusMessageRequested(tr("Connecting to camera stream..."), 3000);
}

void CameraStreamWorker::stopStream(const QString &placeholderText)
{
    const bool hadStream = m_streamReply;

    if (m_streamReply) {
        disconnect(m_streamReply, nullptr, this, nullptr);
        m_streamReply->abort();
        m_streamReply->deleteLater();
        m_streamReply = nullptr;
    }

    resetState();
    if (hadStream || !placeholderText.isEmpty())
        emit streamStopped(placeholderText);
}

void CameraStreamWorker::handleReadyRead()
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

        emit frameReady(frameImage);

        if (!m_receivedFirstFrame) {
            m_receivedFirstFrame = true;
            emit statusMessageRequested(tr("Camera stream active."), 3000);
        }
    }
}

void CameraStreamWorker::handleError(QNetworkReply::NetworkError)
{
    if (!m_streamReply)
        return;

    const QString errorText = tr("Camera stream error: %1").arg(m_streamReply->errorString());
    emit errorOccurred(errorText);
    emit statusMessageRequested(errorText, 5000);
    stopStream();
}

void CameraStreamWorker::handleFinished()
{
    if (!m_streamReply)
        return;

    const bool finishedCleanly = (m_streamReply->error() == QNetworkReply::NoError);
    if (finishedCleanly)
        emit statusMessageRequested(tr("Camera stream ended."), 3000);

    stopStream(finishedCleanly ? tr("Camera stream ended.") : QString());
}

void CameraStreamWorker::resetState()
{
    m_buffer.clear();
    m_receivedFirstFrame = false;
}

// CameraStreamWindow implementation

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

    m_workerThread = new QThread(this);
    m_worker = new CameraStreamWorker();
    m_worker->moveToThread(m_workerThread);
    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &CameraStreamWorker::frameReady, this, &CameraStreamWindow::handleWorkerFrame);
    connect(m_worker, &CameraStreamWorker::statusMessageRequested, this, &CameraStreamWindow::statusMessageRequested);
    connect(m_worker, &CameraStreamWorker::errorOccurred, this, &CameraStreamWindow::handleWorkerError);
    connect(m_worker, &CameraStreamWorker::streamStopped, this, &CameraStreamWindow::handleWorkerStreamStopped);
    m_workerThread->start();

    startStream();
}

CameraStreamWindow::~CameraStreamWindow()
{
    stopStream();

    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait();
    }
}

void CameraStreamWindow::startStream()
{
    if (!m_streamUrl.isValid()) {
        resetPlaceholder(tr("Invalid camera stream URL."));
        emit statusMessageRequested(tr("Camera stream URL is invalid."), 5000);
        return;
    }

    if (!m_worker) {
        resetPlaceholder(tr("Camera stream worker unavailable."));
        emit statusMessageRequested(tr("Unable to start camera stream."), 5000);
        return;
    }

    m_lastFrame = QPixmap();
    resetPlaceholder(tr("Connecting to camera..."));
    emit statusMessageRequested(tr("Connecting to camera stream..."), 3000);
    QMetaObject::invokeMethod(m_worker,
                              "startStream",
                              Qt::QueuedConnection,
                              Q_ARG(QUrl, m_streamUrl));
}

void CameraStreamWindow::stopStream()
{
    if (!m_worker)
        return;

    QMetaObject::invokeMethod(m_worker,
                              "stopStream",
                              Qt::QueuedConnection,
                              Q_ARG(QString, QString()));
}

void CameraStreamWindow::handleWorkerFrame(const QImage &frame)
{
    if (frame.isNull())
        return;

    m_lastFrame = QPixmap::fromImage(frame);
    if (m_videoLabel)
        m_videoLabel->setText(QString());
    updateDisplayedPixmap();
}

void CameraStreamWindow::handleWorkerError(const QString &message)
{
    m_lastFrame = QPixmap();
    resetPlaceholder(message);
}

void CameraStreamWindow::handleWorkerStreamStopped(const QString &placeholderText)
{
    if (!m_videoLabel)
        return;

    if (!placeholderText.isEmpty()) {
        m_lastFrame = QPixmap();
        resetPlaceholder(placeholderText);
        return;
    }

    const bool hasPixmap = m_videoLabel->pixmap() && !m_videoLabel->pixmap()->isNull();
    if (!hasPixmap && m_videoLabel->text().isEmpty())
        resetPlaceholder(tr("Camera stream stopped."));
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
