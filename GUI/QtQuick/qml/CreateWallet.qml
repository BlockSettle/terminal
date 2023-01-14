import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "BsStyles"
import "StyledControls"

CustomPopup {
    id: root

    property var phrase

    objectName: "create_wallet"

    _stack_view.initialItem: terms_conditions
    _arrow_but_visibility: !(terms_conditions.visible || start_create.visible || success_wallet.visible)

    TermsAndConditions {
        id: terms_conditions
        visible: false
        onSig_continue: {
            _stack_view.replace(start_create)
            start_create.init()
        }
    }

    StartCreateWallet {
        id: start_create
        visible: false
        onSig_create_new: {
            root.phrase = bsApp.newSeedPhrase()
            _stack_view.push(wallet_seed)
            wallet_seed.init()
        }
        onSig_import_wallet: {
            _stack_view.push(import_wallet)
            import_wallet.init()
        }
        onSig_hardware_wallet: {
            bsApp.pollHWWallets()
            _stack_view.push(import_hardware)
        }
    }

    WalletSeed {
        id: wallet_seed
        visible: false
        phrase: root.phrase
        onSig_continue: {
            _stack_view.push(wallet_seed_verify)
            wallet_seed_verify.init()
        }
    }

    WalletSeedVerify {
        id: wallet_seed_verify
        visible: false
        phrase: root.phrase
        onSig_verified: {
            _stack_view.push(confirm_password)
            confirm_password.init()
        }
        onSig_skipped: {
            _stack_view.push(wallet_seed_accept)
            wallet_seed_accept.init()
        }
    }

    WalletSeedSkipAccept {
        id: wallet_seed_accept
        visible: false
        onSig_skip: {
            _stack_view.replace(confirm_password)
            confirm_password.init()
        }
        onSig_not_skip: {
            _stack_view.pop()
        }
    }

    ConfirmPassword {
        id: confirm_password
        visible: false
        onSig_confirm: {
            back_arrow_button.visible = false
            _stack_view.push(success_wallet)
        }
    }

    ImportHardware {
        id: import_hardware
        visible: false
        onSig_import: {
            bsApp.importHWWallet(hwDeviceModel.selDevice)
            root.close()
            _stack_view.pop(null)
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
            _stack_view.pop(null)
        }
        onSig_full: {
            _stack_view.replace(import_wallet)
            import_wallet.init()
        }
    }

    ImportWallet {
        id: import_wallet
        visible: false
        onSig_import: {
            _stack_view.push(confirm_password)
            confirm_password.init()
            root.phrase = import_wallet.phrase
        }
        onSig_only_watching: {
            _stack_view.replace(import_watching_wallet)
        }
    }

    SuccessNewWallet {
        id: success_wallet
        visible: false
        onSig_finish: {
            set_visible_arrow(true)
            root.close()
            _stack_view.pop(null)
        }
    }

    function init() {
        if (bsApp.settingActivated === true)
        {
            _stack_view.pop()
            _stack_view.replace(start_create, StackView.Immediate)
            start_create.init()
        }
    }
}

