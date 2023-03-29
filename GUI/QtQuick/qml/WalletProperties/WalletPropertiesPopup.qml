import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import terminal.models 1.0

import "."
import "../BsStyles"
import "../StyledControls"

CustomPopup {
    id: root

    _stack_view.initialItem: properties
    _arrow_but_visibility: !properties.visible

    property var wallet_properties_vm
    property bool walletSeedRequested: false

    Connections {
        target: bsApp
        onWalletSeedAuthFailed: {
            if (_stack_view.currentItem == wallet_seed_auth ||
                _stack_view.currentItem == wallet_seed) {
                _stack_view.replace(wallet_seed_auth)
            }
        }
    }

    x: mainWindow.x + (mainWindow.width - width)/2
    y: mainWindow.y + (mainWindow.height - height)/2

    RenameWallet {
        id: rename_wallet
        visible: false
        wallet_properties_vm: root.wallet_properties_vm

        onSig_confirm: {
            wallet_name.text = rename_wallet.wallet_name;
        }
    }

    ChangePassword {
        id: change_password
        visible: false

        wallet_properties_vm: root.wallet_properties_vm

        onSig_success: {
            _stack_view.push(success)
            success.details_text = qsTr("Your password has successfully been changed")
        }
    }

    ExportWOWallet {
        id: export_wo_wallet
        visible: false

        wallet_properties_vm: root.wallet_properties_vm

        onSig_success: (nameExport, pathExport) => {
            root.close_click()

            success_dialog.details_text = qsTr("Your watching-only wallet has successfully been exported\n\nFilename:\t%1\nFolder:\t%2").arg(nameExport).arg(pathExport)
            
            success_dialog.show()
            success_dialog.raise()
            success_dialog.requestActivate()
        }
    }

    WalletSeedAuth {
        id: wallet_seed_auth
        visible: false
        onAuthorized: {
            _stack_view.replace(wallet_seed)
        }

        wallet_properties_vm: root.wallet_properties_vm
    }

    WalletSeed {
        id: wallet_seed
        visible: false

        wallet_properties_vm: root.wallet_properties_vm
    }

    DeleteWalletWarn {
        id: delete_wallet_warn
        visible: false
        wallet_properties_vm: root.wallet_properties_vm
        onExportWOWallet: {
            _stack_view.push(export_wo_wallet)
        }
        onViewWalletSeed: {
            _stack_view.push(wallet_seed_auth)
            wallet_seed_auth.init()
        }
        onDeleteWallet: {
            _stack_view.push(delete_wallet)
            delete_wallet.init()
        }
    }

    DeleteWallet {
        id: delete_wallet
        visible: false
        onBack: _stack_view.pop()

        wallet_properties_vm: root.wallet_properties_vm

        onWalletDeleted: {
            _stack_view.push(success)
            success.details_text = qsTr("Wallet %1 has successfully been deleted").arg(wallet_properties_vm.walletName)
        }
        onSig_success: {
            _stack_view.push(success)
            success.details_text = qsTr("Wallet has successfully been deleted")
        }
    }

    CustomSuccessWidget {
        id: success

        visible: false
        details_font_size: 16
        details_font_weight: Font.Medium
        onSig_finish: {
            //root.close()
            _stack_view.pop(null)
        }
    }

    CustomSuccessDialog {
        id: success_dialog

        visible: false
        onSig_finish: {
            root.close_click()
        }
    }

    Rectangle {
        id: properties
        height: 548
        width: 580
        color: "transparent"

        Column {
            spacing: 40
            width: parent.width - 48
            height: parent.height - 48
            anchors.centerIn: parent


            Text {
                text: qsTr("Wallet properties")
                color: BSStyle.textColor
                font.family: "Roboto"
                font.pixelSize: 20
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Row {
                width: parent.width - 75
                spacing: 37

                Column {
                    spacing: 8
                    width: parent.width / 2
                    height: parent.height

                    Row {
                        width: parent.width

                        Text {
                            text: qsTr("Wallet name")
                            color: BSStyle.titleTextColor
                            font.family: "Roboto"
                            font.pixelSize: 14
                            width: parent.width / 2
                        }
                        Text {
                            id: wallet_name
                            text: wallet_properties_vm.walletName
                            color: BSStyle.textColor
                            font.family: "Roboto"
                            font.pixelSize: 14
                            width: parent.width / 2
                            horizontalAlignment: Text.AlignRight
                        }
                        
                        Image {
                            id: rename_button

                            x: parent.horizontalCenter - 10
                            y: wallet_name.top

                            source: "qrc:/images/edit_wallet_name.png"
                            width: 32
                            height: 16

                            horizontalAlignment: Image.AlignHCenter
                            fillMode: Image.PreserveAspectFit;
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    rename_wallet.wallet_name = wallet_name.text

                                    rename_wallet.show()
                                    rename_wallet.raise()
                                    rename_wallet.requestActivate()
                                }
                            }
                        }
                    }


                    Row {
                        width: parent.width

                        Text {
                            text: qsTr("Wallet type")
                            color: BSStyle.titleTextColor
                            font.family: "Roboto"
                            font.pixelSize: 14
                            width: parent.width / 2
                        }
                        Text {
                            text: wallet_properties_vm.walletType
                            color: BSStyle.textColor
                            font.family: "Roboto"
                            font.pixelSize: 14
                            width: parent.width / 2
                            horizontalAlignment: Text.AlignRight
                        }
                    }

                    Row {
                        width: parent.width

                        Text {
                            text: qsTr("Wallet ID")
                            color: BSStyle.titleTextColor
                            font.family: "Roboto"
                            font.pixelSize: 14
                            width: parent.width / 2
                        }
                        Text {
                            text: wallet_properties_vm.walletId
                            color: BSStyle.textColor
                            font.family: "Roboto"
                            font.pixelSize: 14
                            width: parent.width / 2
                            horizontalAlignment: Text.AlignRight
                        }
                    }

/*                    Row {
                        width: parent.width

                        Text {
                            text: qsTr("Group / Leaves")
                            color: BSStyle.titleTextColor
                            font.family: "Roboto"
                            font.pixelSize: 14
                            width: parent.width / 2
                        }
                        Text {
                            text: wallet_properties_vm.walletGroups
                            color: BSStyle.textColor
                            font.family: "Roboto"
                            font.pixelSize: 14
                            width: parent.width / 2
                            horizontalAlignment: Text.AlignRight
                        }
                    }*/
                }

                Rectangle {
                    width: 1
                    height: 80
                    color: BSStyle.tableSeparatorColor
                    anchors.verticalCenter: parent.verticalCenter
                }

                Column {
                    spacing: 8
                    width: parent.width / 2
                    height: parent.height

                    Row {
                        width: parent.width

                        Text {
                            text: qsTr("Generated addresses")
                            color: BSStyle.titleTextColor
                            font.family: "Roboto"
                            font.pixelSize: 14
                            width: parent.width / 2
                        }
                        Text {
                            text: wallet_properties_vm.walletGeneratedAddresses
                            color: BSStyle.textColor
                            font.family: "Roboto"
                            font.pixelSize: 14
                            width: parent.width / 2
                            horizontalAlignment: Text.AlignRight
                        }
                    }

                    Row {
                        width: parent.width

                        Text {
                            text: qsTr("Used addresses")
                            color: BSStyle.titleTextColor
                            font.family: "Roboto"
                            font.pixelSize: 14
                            width: parent.width / 2
                        }
                        Text {
                            text: wallet_properties_vm.walletUsedAddresses
                            color: BSStyle.textColor
                            font.family: "Roboto"
                            font.pixelSize: 14
                            width: parent.width / 2
                            horizontalAlignment: Text.AlignRight
                        }
                    }

                    Row {
                        width: parent.width

                        Text {
                            text: qsTr("Available UTXOs")
                            color: BSStyle.titleTextColor
                            font.family: "Roboto"
                            font.pixelSize: 14
                            width: parent.width / 2
                        }
                        Text {
                            text: wallet_properties_vm.walletAvailableUtxo
                            color: BSStyle.textColor
                            font.family: "Roboto"
                            font.pixelSize: 14
                            width: parent.width / 2
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }
            }

            Column {
                spacing: 10
                width: parent.width
                height: parent.height * 0.6
                anchors.margins: 24

                CustomListItem {
                    width: parent.width
                    visible: (!wallet_properties_vm.isHardware && !wallet_properties_vm.isWatchingOnly)

                    icon_source: "qrc:/images/lock_icon.svg"
                    icon_add_source: "qrc:/images/arrow.png"
                    title_text: qsTr("Change password")

                    onClicked: {
                        _stack_view.push(change_password)
                        change_password.init()
                    }
                }

                CustomListItem {
                    width: parent.width

                    icon_source: "qrc:/images/eye_icon.svg"
                    icon_add_source: "qrc:/images/arrow.png"
                    title_text: qsTr("Export watching-only wallet")

                    onClicked: {
                        _stack_view.push(export_wo_wallet)
                    }
                }

                CustomListItem {
                    width: parent.width

                    visible: (!wallet_properties_vm.isHardware && !wallet_properties_vm.isWatchingOnly)

                    icon_source: "qrc:/images/shield_icon.svg"
                    icon_add_source: "qrc:/images/arrow.png"
                    title_text: qsTr("View wallet seed")

                    onClicked: {
                        _stack_view.push(wallet_seed_auth)
                        wallet_seed_auth.init()
                    }
                }

                CustomListItem {
                    width: parent.width

                    icon_source: "qrc:/images/scan_icon.svg"
                    icon_add_source: "qrc:/images/arrow.png"
                    title_text: qsTr("Rescan wallet")

                    onClicked: bsApp.rescanWallet(wallet_properties_vm.walletId)
                }

                CustomListItem {
                    width: parent.width

                    icon_source: "qrc:/images/delete_icon.svg"
                    icon_add_source: "qrc:/images/arrow.png"
                    title_text: qsTr("Delete wallet")

                    onClicked: _stack_view.push(delete_wallet_warn)
                }
            }
        }
    }
}
