import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "StyledControls"
import "BsStyles"


ColumnLayout  {

    id: layout

    signal sig_continue()

    property var phrase

    height: 481
    width: 580
    spacing: 0

    CustomTitleLabel {
        id: title
        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : title.height
        text: "Create new wallet"
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

        Layout.leftMargin: 24
        Layout.bottomMargin: 40

        CustomButton {
            id: copy_seed_but
            text: qsTr("Copy Seed")
            width: 261

            Component.onCompleted: {
                copy_seed_but.preferred = false
            }
            onClicked: {
                bsApp.copySeedToClipboard(phrase)
            }
        }

        CustomButton {
            id: continue_but
            text: qsTr("Continue")
            width: 261

            Component.onCompleted: {
                continue_but.preferred = true
            }

            onClicked: {
                layout.sig_continue()
            }
        }

   }
}
