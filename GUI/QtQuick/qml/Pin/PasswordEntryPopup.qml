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

    x: mainWindow.x + (mainWindow.width - width) / 2
    y: mainWindow.y + 28

    property string device_name
    property bool accept_on_device

    Rectangle {
        id: rect

        color: "#191E2A"
        opacity: 1
        radius: BSSizes.applyScale(16)
        height: BSSizes.applyScale(510)
        width: BSSizes.applyScale(430)
        border.color : BSStyle.defaultBorderColor
        border.width : BSSizes.applyScale(1)

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

            text: qsTr("Enter Password")
        }

        Label {

            id: device_name_title_lbl

            anchors.top: title.bottom
            anchors.topMargin: BSSizes.applyScale(48)
            anchors.left: parent.left
            anchors.leftMargin: BSSizes.applyScale(20)

            text: qsTr("Device name:")

            color: "#45A6FF"

            font.pixelSize: BSSizes.applyScale(16)
            font.family: "Roboto"
            font.weight: Font.Normal
        }

        Label {

            id: device_name_lbl

            anchors.top: title.bottom
            anchors.topMargin: BSSizes.applyScale(48)
            anchors.right: parent.right
            anchors.rightMargin: BSSizes.applyScale(20)

            horizontalAlignment: Text.AlignRight

            text: device_name

            color: "#E2E7FF"

            font.pixelSize: BSSizes.applyScale(16)
            font.family: "Roboto"
            font.weight: Font.Normal
        }

        CustomTextInput {
            id: password

            anchors.top: device_name_lbl.bottom
            anchors.topMargin: BSSizes.applyScale(48)
            anchors.horizontalCenter: parent.horizontalCenter

            //visible: !root.accept_on_device   //always visible now - password can be empty in all cases

            height : BSSizes.applyScale(70)
            width: BSSizes.applyScale(390)

            input_topMargin: BSSizes.applyScale(35)
            title_leftMargin: BSSizes.applyScale(16)
            title_topMargin: BSSizes.applyScale(16)

            title_text: qsTr("Password")

            isPassword: true
            isHiddenText: true
        }

        RowLayout {
            id: row
            spacing: BSSizes.applyScale(10)

            anchors.bottom: parent.bottom
            anchors.bottomMargin: BSSizes.applyScale(40)
            anchors.horizontalCenter: parent.horizontalCenter

            CustomButton {
                id: cancel_but
                text: qsTr("Cancel")
                width: BSSizes.applyScale(190)

                preferred: false
                function click_enter() {
                    root.clean()
                    root.close()
                }
            }

            CustomButton {
                id: accept_but
                text: qsTr("Accept")
                width: BSSizes.applyScale(190)

                //enabled: accept_on_device || password.input_text.length   //always accept now

                preferred: true

                function click_enter() {
                    bsApp.setHWpassword(password.input_text)
                    root.clean()
                    root.close()
                }

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
        password.input_text = ""
        device_name = ""
        accept_on_device = false
    }

}
