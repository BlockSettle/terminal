import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"

Window {
    id: root

    visible: true
    flags: Qt.WindowCloseButtonHint | Qt.FramelessWindowHint | Qt.Dialog
    modality: Qt.WindowModal

    maximumHeight: rect.height
    maximumWidth: rect.width

    minimumHeight: rect.height
    minimumWidth: rect.width

    height: rect.height
    width: rect.width

    color: "transparent"

    x: mainWindow.x + (mainWindow.width - width)/2
    y: mainWindow.y + 28

    property var numbers: [ "7", "8", "9",
                            "4", "5", "6",
                            "1", "2", "3"]
    property string output

    Rectangle {
        id: rect

        color: "#191E2A"
        opacity: 1
        radius: 16
        height: 610
        width: 430
        border.color : BsStyle.defaultBorderColor
        border.width : 1

        Image {
            id: close_button

            anchors.top: parent.top
            anchors.topMargin: 24
            anchors.right: parent.right
            anchors.rightMargin: 24

            source: "qrc:/images/close_button.svg"
            width: 16
            height: 16
            MouseArea {
                anchors.fill: parent
                onClicked: {
                   root.clean()
                   root.close()
                }
            }
        }

        CustomTitleLabel {
            id: title

            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 36

            text: qsTr("Enter PIN")
        }

        Item {
            id: grid_item

            width: 390
            height: 330

            anchors.left: parent.left
            anchors.leftMargin: 25
            anchors.top: title.bottom
            anchors.topMargin: 24

            GridView {
                id: grid

                property bool isComplete: false

                anchors.fill: parent

                cellHeight : 110
                cellWidth : 130

                model: numbers

                interactive: false

                delegate: Button {
                    id: input

                    height: 100
                    width: 120

                    font.pixelSize: 20
                    font.family: "Roboto"
                    font.weight: Font.Normal
                    palette.buttonText: BSStyle.buttonsTextColor

                    text: String.fromCodePoint(0x2022)

                    background: Rectangle {
                        implicitWidth: 100
                        implicitHeight: 120

                        color: input.down ? BSStyle.buttonsStandardPressedColor :
                               (input.hovered ? BSStyle.buttonsStandardHoveredColor : BSStyle.buttonsStandardColor)

                        radius: 14

                        border.color: BSStyle.buttonsStandardBorderColor
                        border.width: input.down? 1 : 0
                    }

                    onClicked: {
                        output += numbers[index]
                        pin_field.text += String.fromCodePoint(0x2022)
                    }
                }

            }
        }


        TextField {
            id: pin_field

            anchors.left: parent.left
            anchors.leftMargin: 25
            anchors.top: grid_item.bottom
            anchors.topMargin: 24

            font.pixelSize: 20
            font.family: "Roboto"
            font.weight: Font.Normal
            color: BSStyle.buttonsTextColor

            leftPadding: 14
            rightPadding: 14

            readOnly: true

            background: Rectangle {
                implicitWidth: 380
                implicitHeight: 46
                color: "#020817"
                radius: 14
            }

            Image {
                id: clear_button

                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                anchors.rightMargin: 24

                source: "qrc:/images/close_button.svg"
                width: 16
                height: 16
                MouseArea {
                    anchors.fill: parent
                    onClicked: clean()
                }
            }
        }

        CustomButton {
            id: accept_but
            text: qsTr("Accept")

            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 40

            width: 380

            Component.onCompleted: {
                accept_but.preferred = true
            }

            function click_enter() {
                if (!accept_but.enabled) return

                bsApp.setHWpin(output)
                clean()
                root.close()
            }
        }

        Keys.onEnterPressed: {
            accept_but.click_enter()
        }

        Keys.onReturnPressed: {
            accept_but.click_enter()
        }

    }

    function init() {
        clean()
    }

    function clean() {
        output = ""
        pin_field.text = ""
    }

}
