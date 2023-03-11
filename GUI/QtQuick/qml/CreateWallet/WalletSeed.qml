import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"

ColumnLayout  {

    id: layout

    signal sig_continue()

    property var phrase

    height: 485
    width: 580
    spacing: 0

    CustomTitleLabel {
        id: title
        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : title.height
        text: qsTr("Create new wallet")
    }

    Label {
        id: subtitle
        Layout.alignment: Qt.AlignCenter
        Layout.topMargin: 16
        Layout.preferredHeight : 16
        text: qsTr("Write down and store your 12 word seed someplace safe and offline")
        color: "#E2E7FF"
        font.pixelSize: 14
        font.family: "Roboto"
        font.weight: Font.Normal
    }

    GridView {
        id: grid

        Layout.fillHeight: true
        Layout.fillWidth: true
        Layout.leftMargin: 25
        Layout.topMargin: 32

        cellHeight : 56
        cellWidth : 180

        model: phrase
        delegate: CustomSeedLabel {
            seed_text: modelData
            serial_num: index + 1
        }
    }

    RowLayout {
        id: row
        spacing: 10

        //Layout.leftMargin: 24
        Layout.bottomMargin: 40
        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

        CustomButton {
            id: copy_seed_but
            text: qsTr("Copy Seed")
            width: 261

            preferred: false

            function click_enter() {
                bsApp.copySeedToClipboard(phrase)
            }
        }

        CustomButton {
            id: continue_but
            text: qsTr("Continue")
            width: 261

            preferred: true

            function click_enter() {
                layout.sig_continue()
            }

        }
    }

    function init()
    {
        continue_but.forceActiveFocus()
    }
}
