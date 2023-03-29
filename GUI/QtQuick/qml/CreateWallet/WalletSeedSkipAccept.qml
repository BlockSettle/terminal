import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"

ColumnLayout  {

    id: layout

    signal sig_skip()
    signal sig_not_skip()

    height: 485
    width: 580

    spacing: 0

    CustomTitleLabel {
        id: title
        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : title.height
        text: qsTr("Verify your seed")
    }

    Image {
        id: warning_icon

        Layout.alignment: Qt.AlignHCenter | Qt.AlignTop
        Layout.topMargin: 24
        Layout.preferredHeight : 44
        Layout.preferredWidth : 44

        source: "qrc:/images/warning_icon.png"
        width: 44
        height: 44
    }

    Label {
        id: warning_description

        text: qsTr("Are you sure you do not want to verify your seed?")

        Layout.alignment: Qt.AlignHCenter | Qt.AlignTop

        Layout.topMargin: 16
        Layout.preferredHeight : 16

        color: "#E2E7FF"
        font.pixelSize: 14
        font.family: "Roboto"
        font.weight: Font.Normal
    }

    Label {
        id: spacer
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    RowLayout {
        id: row
        spacing: 10

        //Layout.leftMargin: 24
        Layout.bottomMargin: 40
        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

        CustomButton {
            id: no_but
            text: qsTr("No")
            width: 261

            preferred: false

            function click_enter() {
                layout.sig_not_skip()
            }

        }

        CustomButton {
            id: skip_but
            text: qsTr("Yes, Skip")
            width: 261

            preferred: true

            function click_enter() {
                layout.sig_skip()
            }
        }
    }

    function init()
    {
        skip_but.forceActiveFocus()
    }
}
