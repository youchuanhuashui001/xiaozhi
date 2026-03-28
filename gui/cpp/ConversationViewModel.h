#ifndef GUI_CONVERSATION_VIEW_MODEL_H
#define GUI_CONVERSATION_VIEW_MODEL_H

#include <QObject>
#include <QString>

class ControlClient;

class ConversationViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString connectionState READ connectionState NOTIFY connectionStateChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString lastUserText READ lastUserText NOTIFY lastUserTextChanged)
    Q_PROPERTY(QString lastAssistantText READ lastAssistantText NOTIFY lastAssistantTextChanged)

public:
    explicit ConversationViewModel(QObject *parent = nullptr);

    QString connectionState() const;
    QString statusText() const;
    QString lastUserText() const;
    QString lastAssistantText() const;

    Q_INVOKABLE void applyEvent(const QString &jsonText);
    void bindClient(ControlClient *client);

signals:
    void connectionStateChanged();
    void statusTextChanged();
    void lastUserTextChanged();
    void lastAssistantTextChanged();

private:
    void setConnectionState(const QString &state);
    void setStatusText(const QString &text);
    void setLastUserText(const QString &text);
    void setLastAssistantText(const QString &text);

    QString m_connectionState;
    QString m_statusText;
    QString m_lastUserText;
    QString m_lastAssistantText;
};

#endif /* GUI_CONVERSATION_VIEW_MODEL_H */
