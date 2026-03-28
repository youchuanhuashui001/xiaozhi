#include "ControlClient.h"

#include <QAbstractSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QWebSocket>

ControlClient::ControlClient(QObject *parent)
    : QObject(parent),
      m_socket(new QWebSocket()),
      m_connectionState(QStringLiteral("idle"))
{
    m_socket->setParent(this);

    connect(m_socket, &QWebSocket::connected, this, &ControlClient::handleSocketConnected);
    connect(m_socket, &QWebSocket::disconnected, this, &ControlClient::handleSocketDisconnected);
    connect(m_socket, &QWebSocket::textMessageReceived, this, &ControlClient::handleTextMessage);
    connect(m_socket, &QWebSocket::errorOccurred, this, &ControlClient::handleSocketError);
}

ControlClient::~ControlClient() = default;

QString ControlClient::endpoint() const
{
    return m_endpoint;
}

void ControlClient::setEndpoint(const QString &endpoint)
{
    if (m_endpoint == endpoint) {
        return;
    }
    m_endpoint = endpoint;
    emit endpointChanged();
}

QString ControlClient::apiKey() const
{
    return m_apiKey;
}

void ControlClient::setApiKey(const QString &apiKey)
{
    if (m_apiKey == apiKey) {
        return;
    }
    m_apiKey = apiKey;
    emit apiKeyChanged();
}

QString ControlClient::connectionState() const
{
    return m_connectionState;
}

void ControlClient::connectToServer()
{
    if (m_endpoint.trimmed().isEmpty()) {
        emit errorOccurred(QStringLiteral("endpoint is empty"));
        return;
    }

    setConnectionState(QStringLiteral("connecting"));
    m_socket->open(QUrl(m_endpoint));
}

void ControlClient::disconnectFromServer()
{
    m_socket->close();
}

void ControlClient::sendCommand(const QString &name,
                                const QString &requestId,
                                const QJsonObject &payload)
{
    QJsonObject obj;

    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        emit errorOccurred(QStringLiteral("control socket is not connected"));
        return;
    }

    obj.insert(QStringLiteral("type"), QStringLiteral("command"));
    obj.insert(QStringLiteral("name"), name);
    if (!requestId.isEmpty()) {
        obj.insert(QStringLiteral("request_id"), requestId);
    }
    obj.insert(QStringLiteral("payload"), payload);

    m_socket->sendTextMessage(QString::fromUtf8(
        QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

void ControlClient::handleSocketConnected()
{
    setConnectionState(QStringLiteral("connected"));
}

void ControlClient::handleSocketDisconnected()
{
    setConnectionState(QStringLiteral("disconnected"));
}

void ControlClient::handleTextMessage(const QString &message)
{
    emit jsonMessageReceived(message);
}

void ControlClient::handleSocketError(QAbstractSocket::SocketError)
{
    setConnectionState(QStringLiteral("error"));
    emit errorOccurred(m_socket->errorString());
}

void ControlClient::setConnectionState(const QString &state)
{
    if (m_connectionState == state) {
        return;
    }
    m_connectionState = state;
    emit connectionStateChanged();
}
