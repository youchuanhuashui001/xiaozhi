import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "pages"

ApplicationWindow {
    id: root
    width: 420
    height: 860
    visible: true
    title: "Xiaozhi Control"
    color: "#F4F5F7"

    property int pageIndex: 0
    property var controlClientRef: (typeof controlClient !== "undefined") ? controlClient : null
    property var viewModelRef: (typeof conversationViewModel !== "undefined") ? conversationViewModel : null

    readonly property var pageTitles: [
        "Connection Settings",
        "Audio & Hardware",
        "Live Conversation"
    ]

    header: Rectangle {
        color: root.color
        height: 74
        border.color: "#E6E8EF"
        border.width: 1

        Text {
            anchors.centerIn: parent
            text: root.pageTitles[root.pageIndex]
            font.pixelSize: 22
            font.bold: true
            color: "#161A27"
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: root.header.height
        anchors.bottomMargin: root.footer.height
        spacing: 0

        StackLayout {
            id: pages
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.pageIndex

            ConnectionSettings {
                controlClient: root.controlClientRef
                viewModel: root.viewModelRef
            }

            AudioHardware {
                controlClient: root.controlClientRef
                viewModel: root.viewModelRef
            }

            LiveConversation {
                controlClient: root.controlClientRef
                viewModel: root.viewModelRef
            }
        }
    }

    footer: Rectangle {
        color: "#FFFFFF"
        height: 84
        border.color: "#E6E8EF"
        border.width: 1

        TabBar {
            id: bar
            anchors.fill: parent
            currentIndex: root.pageIndex
            onCurrentIndexChanged: root.pageIndex = currentIndex

            TabButton {
                text: "Connection"
                font.bold: true
            }

            TabButton {
                text: "Audio"
                font.bold: true
            }

            TabButton {
                text: "Live"
                font.bold: true
            }
        }
    }
}
