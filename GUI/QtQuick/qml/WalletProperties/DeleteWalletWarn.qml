import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"

ColumnLayout {
    id: layout

    height: 548
    width: 580

    spacing: 0

    signal viewWalletSeed()
    signal deleteWallet()

    CustomTitleLabel {
        id: title
        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : title.height
        text: qsTr("Delete wallet")
    }

    Image {

        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : 120
        Layout.preferredWidth : 120

        source: "qrc:/images/wallet_icon_warn.svg"
        width: 120
        height: 120
    }

    CustomTitleLabel {
        font.pixelSize: 14
        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : title.height
        text: qsTr("Save the seed before deleting the wallet")
    }

    Label {
        id: spacer
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    RowLayout {
        Layout.alignment: Qt.AlignHCenter

        CustomButton {
            text: qsTr("View wallet seed")


            Layout.bottomMargin: 40
            Layout.alignment: Qt.AlignBottom

            width: 260

            onClicked: viewWalletSeed()
        }

        CustomButton {
            text: qsTr("Continue")
            preferred: true

            Layout.bottomMargin: 40
            Layout.alignment: Qt.AlignBottom

            width: 260

            onClicked: deleteWallet()
        }
    }
}
