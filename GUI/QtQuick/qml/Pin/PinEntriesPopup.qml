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
    modality: Qt.WindowModality

    maximumHeight: rect.height
    maximumWidth: rect.width

    minimumHeight: rect.height
    minimumWidth: rect.width

    height: rect.height
    width: rect.width

    color: "transparent"

    x: mainWindow.x + (mainWindow.width - width)/2
    y: mainWindow.y + 28

    property var placeholders: ["?", "?", "?", "?", "?",
                                "?", "?", "?", "?"]

    Rectangle {
        id: rect

        color: "#191E2A"
        opacity: 1
        radius: 16
        height: 580
        width: 430
        border.color : "#3C435A"
        border.width : 1

        CustomTitleLabel {
            id: title

            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 36

            text: qsTr("PIN")
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
                property bool isValid: false

                anchors.fill: parent

                cellHeight : 110
                cellWidth : 130

                model: placeholders

                delegate: TextField {
                    id: input

                    focus: true
                    activeFocusOnTab: true

                    placeholderText: qsTr("?")
                    horizontalAlignment: TextInput.AlignHCenter

                    height: 100
                    width: 120

                    color: "#E2E7FF"
                    font.pixelSize: 20
                    font.family: "Roboto"
                    font.weight: Font.Normal

                    validator: IntValidator {
                        bottom: 0
                        top: 9
                    }

                    background: Rectangle {
                        implicitWidth: 100
                        implicitHeight: 100

                        color: "#020817"
                        opacity: 1
                        radius: 14

                        border.color: grid.isValid ? (input.activeFocus ? "#45A6FF" : "#3C435A") : "#EB6060"
                        border.width: 1
                    }

                    onTextEdited: {
                        grid.isComplete = true
                        for (var i = 0; i < grid.count; i++)
                        {
                            if (!grid.itemAtIndex(i).text.length)
                            {
                                grid.isComplete = false
                                break
                            }
                        }
                    }
                }

            }
        }


        Label {
            id: error_description

            visible: !grid.isValid

            text: qsTr("PIN is wrong")

            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: grid_item.bottom
            anchors.topMargin: 24

            height: 16

            color: "#EB6060"
            font.pixelSize: 14
            font.family: "Roboto"
            font.weight: Font.Normal
        }

        CustomButton {
            id: accept_but
            text: qsTr("Accept")

            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 40

            width: 380

            enabled: grid.isComplete

            Component.onCompleted: {
                accept_but.preferred = true
            }

            function click_enter() {
                if (!accept_but.enabled) return
                var res = ""
                for (var i = 0; i < grid.count; i++)
                {
                    res += grid.itemAtIndex(i).text
                }
                bsApp.setHWpin(res)
                root.close()
            }
        }


    }

    function init() {
        grid.isComplete = false
        grid.isValid = true

        for (var i = 0; i < grid.count; i++)
        {
            grid.itemAtIndex(i).text = ""
        }
    }
}
