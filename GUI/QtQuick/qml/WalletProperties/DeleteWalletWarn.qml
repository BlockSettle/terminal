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
    signal exportWOWallet()
    signal deleteSWWallet()
    signal sig_success()

    property var wallet_properties_vm

    Connections
    {
        target:bsApp
        function onSuccessDeleteWallet ()
        {
            if (!layout.visible) {
                return
            }

            layout.sig_success()
        }
        function onFailedDeleteWallet()
        {
            if (!layout.visible) {
                return
            }
            
            showError(qsTr("Failed to delete"))
        }
    }

    CustomMessageDialog {
        id: error_dialog
        error: qsTr("Failed to delete")
        visible: false
    }

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
            visible: !isHW()

            Layout.bottomMargin: 40
            Layout.alignment: Qt.AlignBottom

            width: 260

            onClicked: viewWalletSeed()
        }

        CustomButton {
            text: qsTr("Export watching-only wallet")
            visible: !isHW()

            Layout.bottomMargin: 40
            Layout.alignment: Qt.AlignBottom

            width: 260

            onClicked: exportWOWallet()
        }
    
        CustomButton {
            text: isHW() ? qsTr("Delete") : qsTr("Continue")
            preferred: true

            Layout.bottomMargin: 40
            Layout.alignment: Qt.AlignBottom

            width: 260

            onClicked: {
                if (isHW()) {
                    const result = bsApp.deleteWallet(
                        wallet_properties_vm.walletId,
                        ""
                    )

                    if (result === -1) {
                        showError(qsTr("Failed to delete"))
                    }
                }
                else{
                    deleteSWWallet()
                }
            }
        }
    }

    function isHW(){
        return wallet_properties_vm.isHardware || wallet_properties_vm.isWatchingOnly
    }

    function showError(msg)
    {
        error_dialog.error = msg
        error_dialog.show()
        error_dialog.raise()
        error_dialog.requestActivate()
    }
}
