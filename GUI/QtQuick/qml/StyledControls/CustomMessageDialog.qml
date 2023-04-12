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

    maximumHeight: BSSizes.applyScale(200)
    maximumWidth: BSSizes.applyScale(300)

    minimumHeight: BSSizes.applyScale(200)
    minimumWidth: BSSizes.applyScale(300)

    height: BSSizes.applyScale(250)
    width: BSSizes.applyScale(300)

    color: "transparent"

    x: mainWindow.x + (mainWindow.width - width) / 2
    y: mainWindow.y + (mainWindow.height - height) / 2

    Rectangle {
        id: rect

        color: "#191E2A"
        opacity: 1
        radius: BSSizes.applyScale(16)

        anchors.fill: parent

        border.color : BSStyle.defaultBorderColor
        border.width : BSSizes.applyScale(1)

        Label {
            id: tytleLabel

            anchors.top: rect.top
            anchors.topMargin: BSSizes.applyScale(16)
            anchors.horizontalCenter: rect.horizontalCenter

            color: "#E2E7FF"
            font.pixelSize: BSSizes.applyScale(20)
            font.family: "Roboto"
            font.weight: Font.Medium

            text: "Error"

            horizontalAlignment: Text.AlignHCenter
         }

        Label {
            id: errorLabel

            anchors.top: tytleLabel.bottom
            anchors.topMargin: BSSizes.applyScale(20)
            anchors.horizontalCenter: rect.horizontalCenter

            color: "#E2E7FF"
            font.pixelSize: BSSizes.applyScale(16)
            font.family: "Roboto"
            font.weight: Font.Medium

            text: "Test Description"

            horizontalAlignment: Text.AlignHCenter
            width: ApplicationWindow.width - BSSizes.applyScale(10);
            wrapMode: Label.WordWrap
         }

        CustomButton {
            id: ok_but
            text: qsTr("Ok")

            anchors.bottom: rect.bottom
            anchors.bottomMargin: BSSizes.applyScale(24)
            anchors.horizontalCenter: rect.horizontalCenter

            width: BSSizes.applyScale(250)
            height: BSSizes.applyScale(40)

            preferred: true
            focus:true

            function click_enter() {
                root.close()
            }
        }
    }

}

