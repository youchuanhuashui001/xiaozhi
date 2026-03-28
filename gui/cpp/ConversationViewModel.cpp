#include "ConversationViewModel.h"

#include <QJsonDocument>
#include <QJsonObject>

#include "ControlClient.h"

ConversationViewModel::ConversationViewModel(QObject *parent)
    : QObject(parent),
      m_connectionState(QStringLiteral("idle")),
      m_statusText(QStringLiteral("ready"))
{
}

QString ConversationViewModel::connectionState() const
{
    return m_connectionState;
}

QString ConversationViewModel::statusText() const
{
    return m_statusText;
}

QString ConversationViewModel::lastUserText() const
{
    return m_lastUserText;
}

QString ConversationViewModel::lastAssistantText() const
{
    return m_lastAssistantText;
}

void ConversationViewModel::applyEvent(const QString &jsonText)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonText.toUtf8(), &parseError);
    QJsonObject root;
    QJsonObject payload;

    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        setStatusText(QStringLiteral("invalid event payload"));
        return;
    }

    root = doc.object();
    payload = root.value(QStringLiteral("payload")).toObject();

    if (root.value(QStringLiteral("type")).isString()) {
        const QString eventType = root.value(QStringLiteral("type")).toString();
        if (eventType == QStringLiteral("error")) {
            setStatusText(root.value(QStringLiteral("message")).toString(
                QStringLiteral("unknown control error")));
            return;
        }
        if (eventType == QStringLiteral("command_ack")) {
            const bool ok = root.value(QStringLiteral("ok")).toBool(false);
            setStatusText(ok ? QStringLiteral("command applied")
                             : root.value(QStringLiteral("message")).toString(
                                   QStringLiteral("command failed")));
            return;
        }
    }

    if (!root.value(QStringLiteral("type")).isString() ||
        root.value(QStringLiteral("type")).toString() != QStringLiteral("event")) {
        return;
    }

    if (!root.value(QStringLiteral("name")).isString()) {
        return;
    }

    const QString eventName = root.value(QStringLiteral("name")).toString();

    if (eventName == QStringLiteral("state_changed")) {
        if (payload.value(QStringLiteral("state")).isString()) {
            setConnectionState(payload.value(QStringLiteral("state")).toString());
            setStatusText(QStringLiteral("state updated"));
        }
        return;
    }

    if (eventName == QStringLiteral("stt_result")) {
        if (payload.value(QStringLiteral("text")).isString()) {
            setLastUserText(payload.value(QStringLiteral("text")).toString());
        }
        return;
    }

    if (eventName == QStringLiteral("llm_text")) {
        if (payload.value(QStringLiteral("text")).isString()) {
            setLastAssistantText(payload.value(QStringLiteral("text")).toString());
        }
        return;
    }

    if (eventName == QStringLiteral("tts_state")) {
        if (payload.value(QStringLiteral("state")).isString()) {
            setStatusText(QStringLiteral("tts %1")
                              .arg(payload.value(QStringLiteral("state")).toString()));
        }
    }
}

void ConversationViewModel::bindClient(ControlClient *client)
{
    if (!client) {
        return;
    }

    connect(client, &ControlClient::jsonMessageReceived, this, &ConversationViewModel::applyEvent);
    connect(client, &ControlClient::connectionStateChanged, this, [this, client]() {
        setConnectionState(client->connectionState());
    });
    connect(client, &ControlClient::errorOccurred, this, [this](const QString &message) {
        setStatusText(message);
    });
}

void ConversationViewModel::setConnectionState(const QString &state)
{
    if (m_connectionState == state) {
        return;
    }
    m_connectionState = state;
    emit connectionStateChanged();
}

void ConversationViewModel::setStatusText(const QString &text)
{
    if (m_statusText == text) {
        return;
    }
    m_statusText = text;
    emit statusTextChanged();
}

void ConversationViewModel::setLastUserText(const QString &text)
{
    if (m_lastUserText == text) {
        return;
    }
    m_lastUserText = text;
    emit lastUserTextChanged();
}

void ConversationViewModel::setLastAssistantText(const QString &text)
{
    if (m_lastAssistantText == text) {
        return;
    }
    m_lastAssistantText = text;
    emit lastAssistantTextChanged();
}
