#ifndef CAMERASTREAMWINDOW_H
#define CAMERASTREAMWINDOW_H

#include <QWidget>
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QImage>
#include <QPixmap>
#include <QByteArray>
#include <QUrl>
#include <QPointer>
#include <QString>
#include <QThread>

class QLabel;
class QCloseEvent;
class QResizeEvent;

class CameraStreamWorker : public QObject
{
    Q_OBJECT
public:
    explicit CameraStreamWorker(QObject *parent = nullptr);

public slots:
    void startStream(const QUrl &streamUrl);
    void stopStream(const QString &placeholderText = QString());

signals:
    void frameReady(const QImage &frame);
    void statusMessageRequested(const QString &message, int timeoutMs = 5000);
    void errorOccurred(const QString &message);
    void streamStopped(const QString &placeholderText);

private slots:
    void handleReadyRead();
    void handleError(QNetworkReply::NetworkError code);
    void handleFinished();

private:
    void resetState();

    QNetworkAccessManager *m_networkManager = nullptr;
    QPointer<QNetworkReply> m_streamReply;
    QByteArray m_buffer;
    bool m_receivedFirstFrame = false;
    QUrl m_streamUrl;
};

class CameraStreamWindow : public QWidget
{
    Q_OBJECT
public:
    explicit CameraStreamWindow(const QUrl &streamUrl, QWidget *parent = nullptr);
    ~CameraStreamWindow() override;

signals:
    void windowClosed();
    void statusMessageRequested(const QString &message, int timeoutMs = 5000);

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void handleWorkerFrame(const QImage &frame);
    void handleWorkerError(const QString &message);
    void handleWorkerStreamStopped(const QString &placeholderText);

private:
    void startStream();
    void stopStream();
    void updateDisplayedPixmap();
    void resetPlaceholder(const QString &text);

    QLabel *m_videoLabel = nullptr;
    QPixmap m_lastFrame;
    QUrl m_streamUrl;
    QThread *m_workerThread = nullptr;
    CameraStreamWorker *m_worker = nullptr;
};

#endif // CAMERASTREAMWINDOW_H
