import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"

//import wallet.balance 1.0

ColumnLayout  {

    id: layout

    signal sig_continue(signature: var)
    signal sig_simple()

    height: 748
    width: 1132
    spacing: 0

    RowLayout {

        Layout.fillWidth: true
        Layout.preferredHeight : 34

        Button {
            id: simple_but

            Layout.leftMargin: 24
            Layout.alignment: Qt.AlignLeft | Qt.AlingVCenter

            activeFocusOnTab: true

            font.pixelSize: 13
            font.family: "Roboto"
            font.weight: Font.Normal
            palette.buttonText: "#7A88B0"

            text: qsTr("Simple")

            icon.color: "transparent"
            icon.source: "qrc:/images/advanced_icon.png"
            icon.width: 16
            icon.height: 16

            background: Rectangle {
                implicitWidth: 100
                implicitHeight: 34
                color: "transparent"

                radius: 14

                border.color: "#3C435A"
                border.width: 1

            }

            onClicked: {
               layout.sig_simple()
            }
        }

        CustomTitleLabel {
            id: title

            Layout.leftMargin: 378
            Layout.alignment: Qt.AlignLeft | Qt.AlingVCenter

            text: qsTr("Send Bitcoin")
        }

        Label {
            Layout.fillWidth: true
            Layout.preferredHeight : 34
        }

    }

    RowLayout {

        id: rects_row

        Layout.fillWidth: true
        Layout.preferredHeight : 580
        Layout.topMargin: 20

        spacing: 12

        Rectangle {
            id: inputs_rect

            Layout.leftMargin: 24
            Layout.alignment: Qt.AlignLeft | Qt.AlingVCenter

            width: 536
            height: 580
            color: "transparent"

            radius: 16

            border.color: "#3C435A"
            border.width: 1

            ColumnLayout  {
                id: inputs_layout

                anchors.fill: parent

                spacing: 0

                Label {
                    id: inputs_title

                    Layout.leftMargin: 16
                    Layout.topMargin: 16
                    Layout.alignment: Qt.AlignLeft | Qt.AlingTop

                    text: qsTr("Inputs")

                    height : 19
                    color: "#E2E7FF"
                    font.pixelSize: 16
                    font.family: "Roboto"
                    font.weight: Font.Medium
                }

                Label {
                    Layout.fillWidth: true
                    Layout.fillHeight : true
                }
            }
        }

        Rectangle {
            id: outputs_rect

            Layout.rightMargin: 24
            Layout.alignment: Qt.AlignRight | Qt.AlingVCenter

            width: 536
            height: 580
            color: "transparent"

            radius: 16

            border.color: "#3C435A"
            border.width: 1
        }
    }


    Label {
        Layout.fillWidth: true
        Layout.fillHeight : true
    }


    CustomButton {
        id: broadcast_but

        enabled: false

        width: 1084

        Layout.bottomMargin: 40
        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

        text: qsTr("Broadcast")

        Component.onCompleted: {
            broadcast_but.preferred = true
        }

        function click_enter() {

        }

    }


    function init()
    {
    }
}
