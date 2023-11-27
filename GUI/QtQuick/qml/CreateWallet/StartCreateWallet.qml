import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"


ColumnLayout  {

    id: layout

    signal sig_create_new()
    signal sig_import_wallet()
    signal sig_hardware_wallet()

    height: BSSizes.applyScale(485)
    width: BSSizes.applyScale(580)
    spacing: 0

    CustomTitleLabel {
        id: title
        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : title.height
        text: qsTr("Create new wallet")
    }

    Label {
        Layout.fillWidth: true
        height: BSSizes.applyScale(24)
    }

    Image {
        id: wallet_icon

        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : BSSizes.applyScale(120)
        Layout.preferredWidth : BSSizes.applyScale(120)

        source: "qrc:/images/wallet icon.png"
        width: BSSizes.applyScale(120)
        height: BSSizes.applyScale(120)
    }

    Label {
        Layout.fillWidth: true
        height: BSSizes.applyScale(16)
    }

    Text {
        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : BSSizes.applyScale(16)
        text: "<p style=\"color:\'#7A88B0\'; font-family: \'Roboto\'; font-size:14px; font-weight:400\">Need help? Please consult our <a href=\"https://blocksettle.com/faq\">Getting Started Guides</a></p>"
        color: "#7A88B0"
        onLinkActivated: Qt.openUrlExternally(link)
    }

    Label {
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    RowLayout {
        id: row
        spacing: BSSizes.applyScale(10)

        Layout.bottomMargin: BSSizes.applyScale(40)
        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

        CustomButton {
            id: hardware_but
            text: qsTr("Hardware Wallet")
            width: BSSizes.applyScale(170)

            preferred: false

            function click_enter() {
                sig_hardware_wallet()
            }
        }

        CustomButton {
            id: import_but
            text: qsTr("Import Wallet")
            width: BSSizes.applyScale(170)

            preferred: false

            function click_enter() {
                sig_import_wallet()
            }
        }

        CustomButton {
            id: create_but
            text: qsTr("Create new")
            width: BSSizes.applyScale(170)

            preferred: true

            function click_enter() {
                sig_create_new()
            }
        }
    }

    function init()
    {
        create_but.forceActiveFocus()
    }
}
