#ifndef NETWORK_H
#define NETWORK_H

#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

class GpsNetworkClient : public QObject
{
    Q_OBJECT
public:
    explicit GpsNetworkClient(const QUrl &endpoint, QObject *parent = nullptr);

    void start(int intervalMs = 2000);
    void stop();

signals:
    void coordinateReceived(double latitude, double longitude);
    void networkError(const QString &message);

private slots:
    void fetchCoordinate();
    void handleNetworkReply();

private:
    void scheduleNextRequest();

    QUrl m_endpoint;
    QNetworkAccessManager *m_networkAccessManager = nullptr;
    QTimer *m_timer = nullptr;
    QNetworkReply *m_pendingReply = nullptr;
    int m_intervalMs = 2000;
};

#endif // NETWORK_H
