import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "BsStyles"

Window {
    id: root

    property var phrase

    visible: true
    flags: Qt.WindowCloseButtonHint | Qt.FramelessWindowHint | Qt.Dialog

    maximumHeight: rect.height
    maximumWidth: rect.width

    minimumHeight: rect.height
    minimumWidth: rect.width

    height: rect.height
    width: rect.width

    objectName: "create_wallet"

    color: "transparent"

    x: mainWindow.x + (mainWindow.width - width)/2
    y: mainWindow.y + 28

    Rectangle {
        id: rect

        property var phrase
        color: "#191E2A"
        opacity: 1
        radius: 16
        height: stack_create_wallet.height + 40
        width: stack_create_wallet.width
        border.color : "#3C435A"
        border.width : 1

        Image {
            id: close_button

            anchors.top: parent.top
            anchors.topMargin: 24
            anchors.right: parent.right
            anchors.rightMargin: 24

            source: "qrc:/images/close_button.png"
            width: 16
            height: 16
            MouseArea {
                anchors.fill: parent
                onClicked: {
                   root.close()
                   stack_create_wallet.pop(null)
                }
            }
        }

        Image {
            id: back_arrow_button
            visible: !(terms_conditions.visible || start_create.visible)

            anchors.top: parent.top
            anchors.topMargin: 24
            anchors.left: parent.left
            anchors.leftMargin: 24

            source: "qrc:/images/back_arrow.png"
            width: 20
            height: 16
            MouseArea {
                anchors.fill: parent
                onClicked: {
                   stack_create_wallet.pop()
                }
            }
        }

        StackView {
            id: stack_create_wallet

            initialItem: terms_conditions

            anchors.top: close_button.bottom
            anchors.topMargin: 0

            implicitHeight: currentItem.height
            implicitWidth: currentItem.width


            pushEnter: Transition {
                PropertyAnimation {
                    property: "opacity"
                    from: 0
                    to:1
                    duration: 200
                }
            }

            pushExit: Transition {
                PropertyAnimation {
                    property: "opacity"
                    from: 1
                    to:0
                    duration: 200
                }
            }

            popEnter: Transition {
                PropertyAnimation {
                    property: "opacity"
                    from: 0
                    to:1
                    duration: 200
                }
            }

            popExit: Transition {
                PropertyAnimation {
                    property: "opacity"
                    from: 1
                    to:0
                    duration: 200
                }
            }

            replaceEnter: Transition {
                PropertyAnimation {
                    property: "opacity"
                    from: 0
                    to:1
                    duration: 10
                }
            }

            replaceExit: Transition {
                PropertyAnimation {
                    property: "opacity"
                    from: 1
                    to:0
                    duration: 10
                }
            }
        }

        TermsAndConditions {
            id: terms_conditions
            visible: false
            onSig_continue: {
                stack_create_wallet.replace(start_create)
                start_create.init()
            }
        }

        StartCreateWallet {
            id: start_create
            visible: false
            onSig_create_new: {
                root.phrase = bsApp.newSeedPhrase()
                stack_create_wallet.push(wallet_seed)
                wallet_seed.init()
            }
            onSig_import_wallet: {
                stack_create_wallet.push(import_wallet)
                import_wallet.init()
            }
            onSig_hardware_wallet: {
                bsApp.pollHWWallets()
                stack_create_wallet.push(import_hardware)
            }
        }

        WalletSeed {
            id: wallet_seed
            visible: false
            phrase: root.phrase
            onSig_continue: {
                stack_create_wallet.push(wallet_seed_verify)
                wallet_seed_verify.init()
            }
        }

        WalletSeedVerify {
            id: wallet_seed_verify
            visible: false
            phrase: root.phrase
            onSig_verified: {
                stack_create_wallet.push(confirm_password)
                confirm_password.init()
            }
            onSig_skipped: {
                stack_create_wallet.push(wallet_seed_accept)
                wallet_seed_accept.init()
            }
        }

        WalletSeedSkipAccept {
            id: wallet_seed_accept
            visible: false
            onSig_skip: {
                stack_create_wallet.replace(confirm_password)
                confirm_password.init()
            }
            onSig_not_skip: {
                stack_create_wallet.pop()
            }
        }

        ConfirmPassword {
            id: confirm_password
            visible: false
            onSig_confirm: {
                back_arrow_button.visible = false
                stack_create_wallet.push(success_wallet)
            }
        }

        ImportHardware {
            id: import_hardware
            visible: false
            onSig_import: {
                bsApp.importHWWallet(hwDeviceModel.selDevice)
                root.close()
                stack_create_wallet.pop(null)
            }
            onVisibleChanged: {
                if (!visible) {
                    bsApp.stopHWWalletsPolling()
                }
            }
        }

        ImportWatchingWallet {
            id: import_watching_wallet
            visible: false
            onSig_import: {
                root.close()
                stack_create_wallet.pop(null)
            }
            onSig_full: {
                stack_create_wallet.replace(import_wallet)
                import_wallet.init()
            }
        }

        ImportWallet {
            id: import_wallet
            visible: false
            onSig_import: {
                stack_create_wallet.push(confirm_password)
                confirm_password.init()
                root.phrase = import_wallet.phrase
            }
            onSig_only_watching: {
                stack_create_wallet.replace(import_watching_wallet)
            }
        }

        SuccessNewWallet {
            id: success_wallet
            visible: false
            onSig_finish: {
                back_arrow_button.visible = true
                root.close()
                stack_create_wallet.pop(null)
            }
        }
    }

    function init() {
        if (bsApp.settingActivated === true)
        {
            stack_create_wallet.pop()
            stack_create_wallet.replace(start_create, StackView.Immediate)
            start_create.init()
        }
    }
}

