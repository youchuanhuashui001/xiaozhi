#include <QtTest/QtTest>

#include <QQmlApplicationEngine>

class QmlSmokeTest : public QObject {
    Q_OBJECT

private slots:
    void loadsMainQml();
};

void QmlSmokeTest::loadsMainQml()
{
    QQmlApplicationEngine engine;

    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    QVERIFY2(!engine.rootObjects().isEmpty(), "Main.qml failed to load");
}

QTEST_MAIN(QmlSmokeTest)
#include "test_qml_smoke.moc"
