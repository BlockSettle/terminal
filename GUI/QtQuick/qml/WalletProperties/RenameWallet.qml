import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"

ColumnLayout  {

    id: layout

    height: 548
    width: 580

    anchors.fill: parent

    signal back()

    property var wallet_properties_vm

    CustomMessageDialog {
        id: error_dialog
        error: qsTr("Wallet name already exist")
        visible: false
    }

    CustomTitleLabel {
        id: title
        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : title.height
        text: qsTr("Rename your wallet")
    }

    CustomTextInput {
        id: input

        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : 70
        Layout.preferredWidth: 532
        Layout.topMargin: 10

        input_topMargin: 35
        title_leftMargin: 16
        title_topMargin: 16
        activeFocusOnTab: true

        title_text: qsTr("Wallet Name")

        onEnterPressed: {
            accept_but.click_enter()
        }
        onReturnPressed: {
            accept_but.click_enter()
        }
    }

    Label {
        id: spacer
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    RowLayout {
        id: row
        spacing: 10

        anchors.bottom: parent.bottom
        anchors.bottomMargin: 40
        anchors.horizontalCenter: parent.horizontalCenter

        CustomButton {
            id: cancel_but
            text: qsTr("Cancel")
            width: 260

            preferred: false
            function click_enter() {
                back()
            }
        }

        CustomButton {
            id: accept_but
            text: qsTr("Accept")
            width: 260

            preferred: true

            function click_enter() {
                if (bsApp.isWalletNameExist(input.input_text)) {
                    showError(qsTr("Wallet name already exist"))
                }
                else {
                    if (bsApp.renameWallet(wallet_properties_vm.walletId, input.input_text) === 0) {
                        layout.back()
                    }
                    else {
                        showError(qsTr("Failed to rename wallet"))
                    }
                }
            }

        }
    }

    Keys.onEnterPressed: {
        confirm_but.click_enter()
    }

    Keys.onReturnPressed: {
        confirm_but.click_enter()
    }

    function init()
    {
        input.input_text = wallet_properties_vm.walletName
        input.setActiveFocus()
    }

    function showError(error)
    {
        error_dialog.error = error
        error_dialog.show()
        error_dialog.raise()
        error_dialog.requestActivate()
    }
}
