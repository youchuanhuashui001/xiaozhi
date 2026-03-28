#include <QtTest/QtTest>

#include "ConversationViewModel.h"

class ConversationViewModelTest : public QObject {
    Q_OBJECT

private slots:
    void defaultsToIdle();
    void appliesStateChangedEvent();
    void appliesConversationEvents();
};

void ConversationViewModelTest::defaultsToIdle()
{
    ConversationViewModel vm;

    QCOMPARE(vm.connectionState(), QStringLiteral("idle"));
}

void ConversationViewModelTest::appliesStateChangedEvent()
{
    ConversationViewModel vm;

    vm.applyEvent(QStringLiteral(
        "{\"type\":\"event\",\"name\":\"state_changed\",\"payload\":{\"state\":\"uploading\"}}"));

    QCOMPARE(vm.connectionState(), QStringLiteral("uploading"));
}

void ConversationViewModelTest::appliesConversationEvents()
{
    ConversationViewModel vm;

    vm.applyEvent(QStringLiteral(
        "{\"type\":\"event\",\"name\":\"stt_result\",\"payload\":{\"text\":\"hello\"}}"));
    vm.applyEvent(QStringLiteral(
        "{\"type\":\"event\",\"name\":\"llm_text\",\"payload\":{\"text\":\"world\"}}"));

    QCOMPARE(vm.lastUserText(), QStringLiteral("hello"));
    QCOMPARE(vm.lastAssistantText(), QStringLiteral("world"));
}

QTEST_MAIN(ConversationViewModelTest)
#include "test_view_model.moc"
