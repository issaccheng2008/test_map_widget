#include "network.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

GpsNetworkClient::GpsNetworkClient(const QUrl &endpoint, QObject *parent)
    : QObject(parent)
    , m_endpoint(endpoint)
    , m_networkAccessManager(new QNetworkAccessManager(this))
    , m_timer(new QTimer(this))
{
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &GpsNetworkClient::fetchCoordinate);
}

void GpsNetworkClient::start(int intervalMs)
{
    m_intervalMs = intervalMs;
    if (!m_timer->isActive())
        m_timer->start(0);
}

void GpsNetworkClient::stop()
{
    if (m_timer->isActive())
        m_timer->stop();

    if (m_pendingReply) {
        disconnect(m_pendingReply, nullptr, this, nullptr);
        m_pendingReply->abort();
        m_pendingReply->deleteLater();
        m_pendingReply = nullptr;
    }
}

void GpsNetworkClient::fetchCoordinate()
{
    if (m_pendingReply)
        return;

    if (!m_endpoint.isValid()) {
        emit networkError(tr("GPS endpoint URL is invalid."));
        scheduleNextRequest();
        return;
    }

    QNetworkRequest request(m_endpoint);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("QtGpsClient/1.0"));

    m_pendingReply = m_networkAccessManager->get(request);
    connect(m_pendingReply, &QNetworkReply::finished, this, &GpsNetworkClient::handleNetworkReply);
}

void GpsNetworkClient::handleNetworkReply()
{
    if (!m_pendingReply)
        return;

    QNetworkReply *reply = m_pendingReply;
    m_pendingReply = nullptr;

    const QNetworkReply::NetworkError error = reply->error();
    const QByteArray payload = reply->readAll();

    reply->deleteLater();

    if (error != QNetworkReply::NoError) {
        emit networkError(tr("Network error: %1").arg(reply->errorString()));
        scheduleNextRequest();
        return;
    }

    const auto jsonDoc = QJsonDocument::fromJson(payload);
    if (!jsonDoc.isObject()) {
        emit networkError(tr("Unexpected GPS payload."));
        scheduleNextRequest();
        return;
    }

    const QJsonObject obj = jsonDoc.object();
    const QJsonValue latValue = obj.value(QStringLiteral("latitude"));
    const QJsonValue lonValue = obj.value(QStringLiteral("longitude"));

    if (!latValue.isDouble() || !lonValue.isDouble()) {
        emit networkError(tr("GPS payload missing coordinates."));
        scheduleNextRequest();
        return;
    }

    const double latitude = latValue.toDouble();
    const double longitude = lonValue.toDouble();

    emit coordinateReceived(latitude, longitude);
    scheduleNextRequest();
}

void GpsNetworkClient::scheduleNextRequest()
{
    if (!m_timer->isActive())
        m_timer->start(m_intervalMs);
}
