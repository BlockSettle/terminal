import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"

Window {
    id: root

    property alias error: errorLabel.text

    visible: true
    flags: Qt.WindowCloseButtonHint | Qt.FramelessWindowHint | Qt.Dialog
    modality: Qt.WindowModal

    maximumHeight: 200
    maximumWidth: 300

    minimumHeight: 200
    minimumWidth: 300

    height: 250
    width: 300

    color: "transparent"

    x: mainWindow.x + (mainWindow.width - width)/2
    y: mainWindow.y + (mainWindow.height - height)/2

    Rectangle {
        id: rect

        color: "#191E2A"
        opacity: 1
        radius: 16

        anchors.fill: parent

        border.color : BSStyle.defaultBorderColor
        border.width : 1

        Label {
            id: tytleLabel

            anchors.top: rect.top
            anchors.topMargin: 16
            anchors.horizontalCenter: rect.horizontalCenter

            color: "#E2E7FF"
            font.pixelSize: 20
            font.family: "Roboto"
            font.weight: Font.Medium

            text: "Error"

            horizontalAlignment: Text.AlignHCenter
         }

        Label {
            id: errorLabel

            anchors.top: tytleLabel.bottom
            anchors.topMargin: 20
            anchors.horizontalCenter: rect.horizontalCenter

            color: "#E2E7FF"
            font.pixelSize: 16
            font.family: "Roboto"
            font.weight: Font.Medium

            text: "Test Description"

            horizontalAlignment: Text.AlignHCenter
            width: ApplicationWindow.width - 10;
            wrapMode: Label.WordWrap
         }

        CustomButton {
            id: ok_but
            text: qsTr("Ok")

            anchors.bottom: rect.bottom
            anchors.bottomMargin: 24
            anchors.horizontalCenter: rect.horizontalCenter

            width: 250
            height: 40

            preferred: true

            function click_enter() {
                root.close()
            }
        }

        Keys.onEnterPressed: {
            ok_but.click_enter()
        }

        Keys.onReturnPressed: {
            ok_but.click_enter()
        }

    }

}

