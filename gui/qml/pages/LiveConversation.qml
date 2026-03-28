import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: root
    property var controlClient: null
    property var viewModel: null

    Rectangle {
        anchors.fill: parent
        color: "#F4F5F7"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        Rectangle {
            Layout.fillWidth: true
            radius: 20
            color: "#FFFFFF"
            border.width: 1
            border.color: "#E6E8EF"
            implicitHeight: 140

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 8

                Label {
                    text: "Live Session"
                    font.bold: true
                    font.pixelSize: 20
                    color: "#202839"
                }

                Label {
                    text: "State: " + (root.viewModel ? root.viewModel.connectionState : "idle")
                    color: "#596277"
                }

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: root.viewModel ? root.viewModel.statusText : "ready"
                    color: "#4A3AFF"
                    font.bold: true
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 20
            color: "#FFFFFF"
            border.width: 1
            border.color: "#E6E8EF"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 14

                Rectangle {
                    Layout.fillWidth: true
                    color: "#EEF0F6"
                    radius: 14
                    implicitHeight: 84

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 6

                        Label {
                            text: "User (STT)"
                            color: "#70798D"
                            font.bold: true
                        }

                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: root.viewModel ? root.viewModel.lastUserText : ""
                            color: "#1F2839"
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    color: "#4A3AFF"
                    radius: 14
                    implicitHeight: 96

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 6

                        Label {
                            text: "Xiaozhi (LLM)"
                            color: "#FFFFFF"
                            font.bold: true
                        }

                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: root.viewModel ? root.viewModel.lastAssistantText : ""
                            color: "#FFFFFF"
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    color: "#F1F5FF"
                    radius: 14
                    implicitHeight: 72

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 10

                        Repeater {
                            model: 6
                            Rectangle {
                                width: 5
                                radius: 3
                                color: "#4A3AFF"
                                height: 16 + ((index + 1) % 3) * 10
                                Layout.alignment: Qt.AlignVCenter
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Label {
                            text: "Listening..."
                            color: "#4A3AFF"
                            font.bold: true
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Button {
                Layout.fillWidth: true
                text: "Connect"
                onClicked: {
                    if (root.controlClient) {
                        root.controlClient.connectToServer();
                    }
                }
            }

            Button {
                Layout.fillWidth: true
                text: "Disconnect"
                onClicked: {
                    if (root.controlClient) {
                        root.controlClient.sendCommand("disconnect_server");
                        root.controlClient.disconnectFromServer();
                    }
                }
            }
        }
    }
}
