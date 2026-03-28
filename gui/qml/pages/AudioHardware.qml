import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: root
    property var controlClient: null
    property var viewModel: null

    function pushAudioConfig() {
        if (!root.controlClient) {
            return;
        }
        root.controlClient.sendCommand("set_audio_config", "", {
            "input_gain": Math.round(inputGain.value),
            "output_volume": Math.round(outputVolume.value),
            "noise_cancellation": noiseSwitch.checked
        });
    }

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
            anchors.margins: 20
            spacing: 16

            Rectangle {
                Layout.fillWidth: true
                radius: 18
                color: "#FFFFFF"
                border.width: 1
                border.color: "#E6E8EF"
                implicitHeight: 260

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 14

                    Label {
                        text: "Microphone"
                        font.bold: true
                        color: "#232B3C"
                    }

                    Label {
                        text: "Input Device: alsa: hw:0,0"
                        color: "#616B7F"
                    }

                    Label {
                        text: "Input Gain: " + Math.round(inputGain.value) + "%"
                        color: "#232B3C"
                        font.bold: true
                    }

                    Slider {
                        id: inputGain
                        Layout.fillWidth: true
                        from: 0
                        to: 100
                        value: 75
                        onMoved: root.pushAudioConfig()
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Label {
                            text: "Noise Cancellation"
                            color: "#232B3C"
                            font.bold: true
                            Layout.fillWidth: true
                        }

                        Switch {
                            id: noiseSwitch
                            checked: true
                            onToggled: root.pushAudioConfig()
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
                implicitHeight: 200

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 14

                    Label {
                        text: "Speaker"
                        font.bold: true
                        color: "#232B3C"
                    }

                    Label {
                        text: "Output Device: sysdefault:CARD=PCH"
                        color: "#616B7F"
                    }

                    Label {
                        text: "Output Volume: " + Math.round(outputVolume.value) + "%"
                        color: "#232B3C"
                        font.bold: true
                    }

                    Slider {
                        id: outputVolume
                        Layout.fillWidth: true
                        from: 0
                        to: 100
                        value: 40
                        onMoved: root.pushAudioConfig()
                    }
                }
            }
        }
    }
}
