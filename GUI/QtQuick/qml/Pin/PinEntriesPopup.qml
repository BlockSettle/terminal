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
    y: mainWindow.y + BSSizes.applyScale(28)

    property var numbers: [ "7", "8", "9",
                            "4", "5", "6",
                            "1", "2", "3"]
    property string output

    Rectangle {
        id: rect

        color: "#191E2A"
        opacity: 1
        radius: BSSizes.applyScale(16)
        height: BSSizes.applyScale(610)
        width: BSSizes.applyScale(430)
        border.color : BSStyle.defaultBorderColor
        border.width : 1

        Image {
            id: close_button

            anchors.top: parent.top
            anchors.topMargin: BSSizes.applyScale(24)
            anchors.right: parent.right
            anchors.rightMargin: BSSizes.applyScale(24)

            source: "qrc:/images/close_button.svg"
            width: BSSizes.applyScale(16)
            height: BSSizes.applyScale(16)
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
            anchors.topMargin: BSSizes.applyScale(36)

            text: qsTr("Enter PIN")
        }

        Item {
            id: grid_item

            width: BSSizes.applyScale(390)
            height: BSSizes.applyScale(330)

            anchors.left: parent.left
            anchors.leftMargin: BSSizes.applyScale(25)
            anchors.top: title.bottom
            anchors.topMargin: BSSizes.applyScale(24)

            GridView {
                id: grid

                property bool isComplete: false

                anchors.fill: parent

                cellHeight : BSSizes.applyScale(110)
                cellWidth : BSSizes.applyScale(130)

                model: numbers

                interactive: false

                delegate: Button {
                    id: input

                    height: BSSizes.applyScale(100)
                    width: BSSizes.applyScale(120)

                    font.pixelSize: BSSizes.applyScale(20)
                    font.family: "Roboto"
                    font.weight: Font.Normal
                    palette.buttonText: BSStyle.buttonsTextColor

                    text: String.fromCodePoint(0x2022)

                    background: Rectangle {
                        implicitWidth: BSSizes.applyScale(100)
                        implicitHeight: BSSizes.applyScale(120)

                        color: input.down ? BSStyle.buttonsStandardPressedColor :
                               (input.hovered ? BSStyle.buttonsStandardHoveredColor : BSStyle.buttonsStandardColor)

                        radius: BSSizes.applyScale(14)

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
            anchors.leftMargin: BSSizes.applyScale(25)
            anchors.top: grid_item.bottom
            anchors.topMargin: BSSizes.applyScale(24)

            font.pixelSize: BSSizes.applyScale(20)
            font.family: "Roboto"
            font.weight: Font.Normal
            color: BSStyle.buttonsTextColor

            leftPadding: BSSizes.applyScale(14)
            rightPadding: BSSizes.applyScale(14)

            readOnly: true

            background: Rectangle {
                implicitWidth: BSSizes.applyScale(380)
                implicitHeight: BSSizes.applyScale(46)
                color: "#020817"
                radius: BSSizes.applyScale(14)
            }

            Image {
                id: clear_button

                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                anchors.rightMargin: BSSizes.applyScale(24)

                source: "qrc:/images/close_button.svg"
                width: BSSizes.applyScale(16)
                height: BSSizes.applyScale(16)
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
            anchors.bottomMargin: BSSizes.applyScale(40)

            width: BSSizes.applyScale(380)

            preferred: true

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
