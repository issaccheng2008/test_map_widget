#ifndef CAMERASTREAMWINDOW_H
#define CAMERASTREAMWINDOW_H

#include <QWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPixmap>
#include <QByteArray>
#include <QUrl>
#include <QPointer>
#include <QString>

class QLabel;
class QCloseEvent;
class QResizeEvent;

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
    void handleReadyRead();
    void handleError(QNetworkReply::NetworkError code);
    void handleFinished();

private:
    void startStream();
    void stopStream();
    void updateDisplayedPixmap();
    void resetPlaceholder(const QString &text);

    QLabel *m_videoLabel = nullptr;
    QNetworkAccessManager m_networkManager;
    QPointer<QNetworkReply> m_streamReply;
    QByteArray m_buffer;
    QPixmap m_lastFrame;
    QUrl m_streamUrl;
    bool m_receivedFirstFrame = false;
};

#endif // CAMERASTREAMWINDOW_H
