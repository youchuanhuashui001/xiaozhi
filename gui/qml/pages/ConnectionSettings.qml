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

    ScrollView {
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 16
            anchors.margins: 20

            Rectangle {
                Layout.fillWidth: true
                radius: 18
                color: "#FFFFFF"
                border.width: 1
                border.color: "#E6E8EF"
                implicitHeight: 280

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 12

                    Label {
                        text: "Server Endpoint URL"
                        font.bold: true
                        color: "#222A3A"
                    }

                    TextField {
                        id: endpointField
                        Layout.fillWidth: true
                        placeholderText: "wss://127.0.0.1:8000"
                        text: root.controlClient ? root.controlClient.endpoint : ""
                        font.pixelSize: 16
                    }

                    Label {
                        text: "WebSocket API Key"
                        font.bold: true
                        color: "#222A3A"
                    }

                    TextField {
                        id: apiKeyField
                        Layout.fillWidth: true
                        placeholderText: "Enter API key"
                        echoMode: TextInput.Password
                        text: root.controlClient ? root.controlClient.apiKey : ""
                        font.pixelSize: 16
                    }

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: root.controlClient
                              ? "Connection: " + root.controlClient.connectionState
                              : "Connection: unavailable"
                        color: "#6D7486"
                    }

                    Button {
                        Layout.fillWidth: true
                        text: "Test Connection"

                        background: Rectangle {
                            radius: 14
                            color: "#4A3AFF"
                        }

                        contentItem: Text {
                            text: parent.text
                            color: "#FFFFFF"
                            font.pixelSize: 16
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: {
                            if (!root.controlClient) {
                                return;
                            }

                            root.controlClient.endpoint = endpointField.text;
                            root.controlClient.apiKey = apiKeyField.text;

                            if (root.controlClient.connectionState === "connected") {
                                root.controlClient.sendCommand("test_connection");
                            } else {
                                root.controlClient.connectToServer();
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 18
                color: "#FFFFFF"
                border.width: 1
                border.color: "#E6E8EF"
                implicitHeight: 120

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 8

                    Label {
                        text: "Runtime Status"
                        font.bold: true
                        color: "#222A3A"
                    }

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: root.viewModel ? root.viewModel.statusText : "No status"
                        color: "#4D5466"
                    }
                }
            }
        }
    }
}
