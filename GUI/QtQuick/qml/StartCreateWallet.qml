import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "StyledControls"
import "BsStyles"


ColumnLayout  {

    id: layout

    signal sig_create_new()
    signal sig_import_wallet()
    signal sig_hardware_wallet()

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
        Layout.fillWidth: true
        height: 24
    }

    Image {
        id: wallet_icon

        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : 120
        Layout.preferredWidth : 120

        source: "qrc:/images/wallet icon.png"
        width: 120
        height: 120
    }

    Label {
        Layout.fillWidth: true
        height: 16
    }

    Text {
        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : 16
        text: "<p style=\"color:#7A88B0; font-family: \'Roboto\'; font-size:14px; font-weight:400\">Need help? Please consult our <a href=\"https://blocksettle.com/faq\">Getting Started Guides</a></p>"
        onLinkActivated: Qt.openUrlExternally(link)
    }

    Label {
        Layout.fillWidth: true
        height: 196
    }

    RowLayout {
        id: row
        spacing: 10

        CustomButton {
            id: hardware_but
            text: qsTr("Hardware Wallet")
            Layout.leftMargin: 25
            width: 170

            Component.onCompleted: {
                hardware_but.preferred = false
            }
            onClicked: {
                sig_hardware_wallet()
            }
        }

        CustomButton {
            id: import_but
            text: qsTr("Import Wallet")
            width: 170

            Component.onCompleted: {
                import_but.preferred = false
            }
            onClicked: {
                sig_import_wallet()
            }
        }

        CustomButton {
            id: create_but
            text: qsTr("Create new")
            width: 170

            Component.onCompleted: {
                create_but.preferred = true
            }
            onClicked: {
                sig_create_new()
            }
        }
    }
}
