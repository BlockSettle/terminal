import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"

import wallet.balance 1.0

ColumnLayout  {

    id: layout

    signal sig_finish()

    height: 549
    width: 580
    spacing: 0

    CustomTitleLabel {
        id: title
        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : title.height
        text: qsTr("Generate address")
    }


    Label {
        id: subtitle
        Layout.alignment: Qt.AlignCenter
        Layout.topMargin: 16
        Layout.preferredHeight : 16
        text: qsTr("Bitcoins sent to this address will appear in:")
        color: "#7A88B0"
        font.pixelSize: 14
        font.family: "Roboto"
        font.weight: Font.Normal
    }


    Label {
        id: wallet_name
        Layout.alignment: Qt.AlignCenter
        Layout.topMargin: 6
        Layout.preferredHeight : 16
        text:  getWalletData(overviewWalletIndex, WalletBalance.NameRole) + " / Native SegWit"
        color: "#E2E7FF"
        font.pixelSize: 14
        font.family: "Roboto"
        font.weight: Font.Normal

        Connections
        {
            target:walletBalances
            function onRowCountChanged ()
            {
                wallet_name.text = getWalletData(overviewWalletIndex, WalletBalance.NameRole) + " / Native SegWit"
            }
        }
    }


    Image {
        id: wallet_icon

        Layout.topMargin: 48
        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        Layout.preferredHeight : 214
        Layout.preferredWidth : 214

        source: "image://QR/" + bsApp.generatedAddress
        sourceSize.width: 214
        sourceSize.height: 214
        width: 214
        height: 214
    }

    Label {
        Layout.topMargin: 30
        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        text: bsApp.generatedAddress
        font.pixelSize: 16
        font.family: "Roboto"
        font.weight: Font.Medium
        color: "#E2E7FF"
    }

    Label {
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    CustomButton {
        id: copy_but

        width: 530

        Layout.bottomMargin: 40
        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

        text: qsTr("Copy to clipboard")

        icon.source: "qrc:/images/copy_icon.svg"
        icon.width: 24
        icon.height: 24
        icon.color: "#FFFFFF"

        Component.onCompleted: {
            copy_but.preferred = true
        }

        function click_enter() {
            bsApp.copySeedToClipboard(bsApp.generatedAddress)
        }

    }
}
