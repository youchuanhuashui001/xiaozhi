#ifndef GUI_CONTROL_CLIENT_H
#define GUI_CONTROL_CLIENT_H

#include <QObject>
#include <QJsonObject>
#include <QString>
#include <QVariantMap>

class QAbstractSocket;
class QWebSocket;

class ControlClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString endpoint READ endpoint WRITE setEndpoint NOTIFY endpointChanged)
    Q_PROPERTY(QString apiKey READ apiKey WRITE setApiKey NOTIFY apiKeyChanged)
    Q_PROPERTY(QString connectionState READ connectionState NOTIFY connectionStateChanged)

public:
    explicit ControlClient(QObject *parent = nullptr);
    ~ControlClient() override;

    QString endpoint() const;
    void setEndpoint(const QString &endpoint);

    QString apiKey() const;
    void setApiKey(const QString &apiKey);

    QString connectionState() const;

    Q_INVOKABLE void connectToServer();
    Q_INVOKABLE void disconnectFromServer();
    Q_INVOKABLE void sendCommand(const QString &name,
                                 const QString &requestId = QString(),
                                 const QVariantMap &payload = QVariantMap());

signals:
    void endpointChanged();
    void apiKeyChanged();
    void connectionStateChanged();
    void jsonMessageReceived(const QString &jsonText);
    void errorOccurred(const QString &message);

private slots:
    void handleSocketConnected();
    void handleSocketDisconnected();
    void handleTextMessage(const QString &message);
    void handleSocketError(QAbstractSocket::SocketError error);

private:
    void setConnectionState(const QString &state);

    QWebSocket *m_socket;
    QString m_endpoint;
    QString m_apiKey;
    QString m_connectionState;
};

#endif /* GUI_CONTROL_CLIENT_H */
