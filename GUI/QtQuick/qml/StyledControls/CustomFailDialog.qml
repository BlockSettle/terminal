import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"

Window  {
    id: root

    property alias header: title.text
    property alias fail: details.text

    visible: true
    flags: Qt.WindowCloseButtonHint | Qt.FramelessWindowHint | Qt.Dialog
    modality: Qt.WindowModal

    height: BSSizes.applyScale(375)
    width: BSSizes.applyScale(380)

    color: "transparent"

    x: mainWindow.x + (mainWindow.width - width)/2
    y: mainWindow.y + (mainWindow.height - height)/2

    Rectangle {
        id: rect

        color: "#191E2A"
        opacity: 1
        radius: BSSizes.applyScale(16)

        anchors.fill: parent

        border.color : BSStyle.defaultBorderColor
        border.width : BSSizes.applyScale(1)

        Image {
            id: close_button

            anchors.top: parent.top
            anchors.topMargin: BSSizes.applyScale(20)
            anchors.right: parent.right
            anchors.rightMargin: BSSizes.applyScale(22)

            source: "qrc:/images/close_button.svg"
            width: BSSizes.applyScale(16)
            height: BSSizes.applyScale(16)
            MouseArea {
                anchors.fill: parent
                onClicked: {
                   root.close()
                }
            }
        }

        ColumnLayout  {
            id: layout
        
            anchors.fill: parent
    
            CustomTitleLabel {
                Layout.topMargin: BSSizes.applyScale(36)
                id: title
                Layout.alignment: Qt.AlignCenter
                Layout.preferredHeight : title.height
                text: qsTr("Incorrect Password")
            }


            Image {
                Layout.topMargin: BSSizes.applyScale(24)
                Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
                Layout.preferredHeight : BSSizes.applyScale(120)
                Layout.preferredWidth : BSSizes.applyScale(120)

                source: "qrc:/images/try_icon.png"
                width: BSSizes.applyScale(120)
                height: BSSizes.applyScale(120)
            }


            Label {
                id: details

                Layout.topMargin: BSSizes.applyScale(16)
                Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
                text: qsTr("The password you entered is incorrect")
                font.pixelSize: BSSizes.applyScale(14)
                font.family: "Roboto"
                font.weight: Font.Normal
                color: "#E2E7FF"
            }

            Label {
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            CustomButton {
                id: finish_but
                text: qsTr("Try again")

                width: BSSizes.applyScale(186)

                Layout.bottomMargin: BSSizes.applyScale(40)
                Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

                preferred: true
                focus:true

                function click_enter() {
                    root.close()
                }
            }
        }
    }
}
