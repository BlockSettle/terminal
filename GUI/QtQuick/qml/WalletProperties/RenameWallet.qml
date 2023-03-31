import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"

CustomPopup {
    id: root

    property alias header: title.text
    property alias wallet_name: input.input_text
    property var wallet_properties_vm

    signal sig_confirm()

    visible: true
    flags: Qt.WindowCloseButtonHint | Qt.FramelessWindowHint | Qt.Dialog
    modality: Qt.WindowModal

    height: 375
    width: 400

    color: "transparent"

    x: mainWindow.x + (mainWindow.width - width)/2
    y: mainWindow.y + (mainWindow.height - height)/2

    CustomMessageDialog {
        id: error_dialog
        error: qsTr("Wallet name already exist")
        visible: false
    }


    Rectangle {
        id: rect

        color: "#191E2A"
        opacity: 1
        radius: 16

        anchors.fill: parent

        border.color : BSStyle.defaultBorderColor
        border.width : 1

        ColumnLayout  {

            id: layout

            anchors.fill: parent

            CustomTitleLabel {
                id: title
                Layout.topMargin: 32
                Layout.alignment: Qt.AlignCenter
                Layout.preferredHeight : title.height
                text: qsTr("Rename your wallet")
            }

            CustomTextInput {
                id: input

                Layout.alignment: Qt.AlignCenter
                Layout.preferredHeight : 70
                Layout.preferredWidth: 375
                Layout.topMargin: 32

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
                    width: 190

                    preferred: false
                    function click_enter() {
                        //root.clean()
                        root.close()
                    }
                }

                CustomButton {
                    id: accept_but
                    text: qsTr("Accept")
                    width: 190

                    preferred: true

                    function click_enter() {
                        if (bsApp.isWalletNameExist(input.input_text)) {
                            showError(qsTr("Wallet name already exist"))
                        }
                        else {
                            if (bsApp.renameWallet(wallet_properties_vm.walletId, input.input_text) === 0) {
                                root.sig_confirm()
                                root.close()
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
                input.setActiveFocus()
            }

            function showError(error)
            {
                error_dialog.error = error
                error_dialog.show()
                error_dialog.raise()
                error_dialog.requestActivate()
                init()
            }
        }
    }
}