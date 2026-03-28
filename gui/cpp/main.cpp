#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUrl>

#include "ControlClient.h"
#include "ConversationViewModel.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;
    ControlClient controlClient;
    ConversationViewModel viewModel;

    viewModel.bindClient(&controlClient);
    engine.rootContext()->setContextProperty(QStringLiteral("controlClient"), &controlClient);
    engine.rootContext()->setContextProperty(QStringLiteral("conversationViewModel"), &viewModel);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}
