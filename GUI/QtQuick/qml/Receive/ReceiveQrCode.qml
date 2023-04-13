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

    height: BSSizes.applyWindowHeightScale(549)
    width: BSSizes.applyWindowWidthScale(580)
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
        Layout.topMargin: BSSizes.applyScale(16)
        Layout.preferredHeight : BSSizes.applyScale(16)
        text: qsTr("Bitcoins sent to this address will appear in:")
        color: "#7A88B0"
        font.pixelSize: BSSizes.applyScale(14)
        font.family: "Roboto"
        font.weight: Font.Normal
    }


    Label {
        id: wallet_name
        Layout.alignment: Qt.AlignCenter
        Layout.topMargin: BSSizes.applyScale(6)
        Layout.preferredHeight : BSSizes.applyScale(16)
        text:  qsTr("%1 / Native SegWit").arg(getWalletData(walletBalances.selectedWallet, WalletBalance.NameRole))
        color: "#E2E7FF"
        font.pixelSize: BSSizes.applyScale(14)
        font.family: "Roboto"
        font.weight: Font.Normal
    }


    Image {
        id: wallet_icon

        Layout.topMargin: BSSizes.applyScale(48)
        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        Layout.preferredHeight : BSSizes.applyScale(214)
        Layout.preferredWidth : BSSizes.applyScale(214)

        source: bsApp.generatedAddress !== "" ? ("image://QR/" + bsApp.generatedAddress) : ""
        sourceSize.width: BSSizes.applyScale(214)
        sourceSize.height: BSSizes.applyScale(214)
        width: BSSizes.applyScale(214)
        height: BSSizes.applyScale(214)
    }

    Label {
        Layout.topMargin: BSSizes.applyScale(30)
        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        text: bsApp.generatedAddress
        font.pixelSize: BSSizes.applyScale(16)
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

        width: BSSizes.applyScale(530)

        Layout.bottomMargin: BSSizes.applyScale(40)
        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

        text: qsTr("Copy to clipboard")

        icon.source: "qrc:/images/copy_icon.svg"
        icon.width: BSSizes.applyScale(24)
        icon.height: BSSizes.applyScale(24)
        icon.color: "#FFFFFF"

        preferred: true

        function click_enter() {
            bsApp.copySeedToClipboard(bsApp.generatedAddress)
        }

    }
}
