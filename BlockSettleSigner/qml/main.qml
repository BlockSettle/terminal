import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import QtQuick.Window 2.1
import Qt.labs.settings 1.0

import com.blocksettle.WalletsProxy 1.0
import com.blocksettle.TXInfo 1.0
import com.blocksettle.WalletInfo 1.0
import com.blocksettle.AutheIDClient 1.0
import com.blocksettle.AuthSignWalletObject 1.0
import com.blocksettle.QSeed 1.0
import com.blocksettle.QPasswordData 1.0
import com.blocksettle.WalletsViewModel 1.0
import com.blocksettle.NsWallet.namespace 1.0

import "StyledControls"
import "BsStyles"
import "BsControls"
import "BsDialogs"
import "js/helper.js" as JsHelper


ApplicationWindow {
    id: mainWindow

    visible: true
    title: qsTr("BlockSettle Signer")
    width: 800
    height: 600
    minimumWidth: 800
    minimumHeight: 600

    background: Rectangle {
        color: BSStyle.backgroundColor
    }
    overlay.modal: Rectangle {
        color: BSStyle.backgroundModalColor
    }
    overlay.modeless: Rectangle {
        color: BSStyle.backgroundModeLessColor
    }

    // attached to use from c++
    function messageBoxCritical(title, text, details) {
        return JsHelper.messageBoxCritical(title, text, details)
    }

    Settings {
        id: settings
        category: "GUI"
        property alias x: mainWindow.x
        property alias y: mainWindow.y
        property alias width: mainWindow.width
        property alias height: mainWindow.height
        property alias tabIdx: swipeView.currentIndex
    }

    InfoBanner {
        id: ibSuccess
        bgColor: "darkgreen"
    }
    InfoBanner {
        id: ibFailure
        bgColor: "darkred"
    }

    DirSelectionDialog {
        id: ldrWoWalletDirDlg
        title: qsTr("Select watching only wallet target directory")
    }
    DirSelectionDialog {
        id: ldrDirDlg
        title: qsTr("Select directory")
    }

    SwipeView {
        id: swipeView
        anchors.fill: parent
        currentIndex: tabBar.currentIndex

        StatusPage {
            id: dashboardPage
        }

        SettingsPage {
            id: settingsPage
        }

        AutoSignPage {
            id: autoSignPage
        }

        WalletsPage {
            id: walletsPage
        }
    }

    footer: TabBar {
        id: tabBar
        currentIndex: swipeView.currentIndex
        height: 50
        spacing: 2;
        background: Rectangle {
            color: "transparent"
        }

        CustomTabButton {
            id: btnStatus
            text: qsTr("Dashboard")

        }

        CustomTabButton {
            id: btnSettings
            text: qsTr("Settings")

        }

        CustomTabButton {
            id: btnAutoSign
            text: qsTr("Auto-Sign")

        }

        CustomTabButton {
            id: btnWallets
            text: qsTr("Wallets")

        }
    }

    onClosing: {
        settingsPage.storeSettings();
        autoSignPage.storeSettings();
    }

    signal passwordEntered(string walletId, QPasswordData passwordData, bool cancelledByUser)

    function createTxSignDialog(prompt, txInfo, walletInfo) {
        // called from QMLAppObj::requestPassword

        var dlg = Qt.createComponent("BsDialogs/TxSignDialog.qml").createObject(mainWindow)
        dlg.walletInfo = walletInfo
        dlg.prompt = prompt
        dlg.txInfo = txInfo

        dlg.accepted.connect(function() {
            passwordEntered(walletInfo.walletId, dlg.passwordData, false)
        })
        dlg.rejected.connect(function() {
            passwordEntered(walletInfo.walletId, dlg.passwordData, true)
        })
        mainWindow.requestActivate()
        dlg.open()

        dlg.init()
    }

    function raiseWindow() {
        JsHelper.raiseWindow()
    }

    function createNewWalletDialog(data) {
        var newSeed = qmlFactory.createSeed(signerSettings.testNet)

        // allow user to save wallet seed lines and then prompt him to enter them for verification
        var dlgNewSeed = Qt.createComponent("BsDialogs/WalletNewSeedDialog.qml").createObject(mainWindow)
        dlgNewSeed.seed = newSeed
        dlgNewSeed.accepted.connect(function() {
            // let user set a password or Auth eID and also name and desc. for the new wallet
            var dlgCreateWallet = Qt.createComponent("BsDialogs/WalletCreateDialog.qml").createObject(mainWindow)
            dlgCreateWallet.primaryWalletExists = walletsProxy.primaryWalletExists

            dlgCreateWallet.seed = newSeed
            dlgCreateWallet.open();
        })
        dlgNewSeed.open()
    }

    function importWalletDialog(data) {
        var dlgImp = Qt.createComponent("BsDialogs/WalletImportDialog.qml").createObject(mainWindow)
        dlgImp.primaryWalletExists = walletsProxy.primaryWalletExists
        dlgImp.open()
    }

    function backupWalletDialog(walletId) {
        var dlg = Qt.createComponent("BsDialogs/WalletBackupDialog.qml").createObject(mainWindow)
        dlg.walletInfo = qmlFactory.createWalletInfo(walletId)
        dlg.targetDir = signerSettings.dirDocuments
        dlg.open()
    }

    function deleteWalletDialog(walletId) {
        var dlg = Qt.createComponent("BsDialogs/WalletDeleteDialog.qml").createObject(mainWindow)
        dlg.walletInfo = qmlFactory.createWalletInfo(walletId)
        dlg.rootName = walletsProxy.getRootWalletName(walletId)

        dlg.accepted.connect(function() {
            if (dlg.backup) {
                var dlgBkp = Qt.createComponent("BsDialogs/WalletBackupDialog.qml").createObject(mainWindow)
                dlgBkp.walletInfo = qmlFactory.createWalletInfo(walletId)
                dlgBkp.targetDir = signerSettings.dirDocuments
                dlgBkp.accepted.connect(function() {
                    if (walletsProxy.deleteWallet(walletId)) {
                        JsHelper.messageBox(BSMessageBox.Type.Success
                                            , qsTr("Wallet")
                                            , qsTr("Wallet successfully deleted.")
                                            , qsTr("Wallet Name: %1\nWallet ID: %2").arg(dlg.walletInfo.name).arg(walletId))
                    }
                })
                dlgBkp.open()
            }
            else {
                if (walletsProxy.deleteWallet(walletId)) {
                    JsHelper.messageBox(BSMessageBox.Type.Success
                                        , qsTr("Wallet")
                                        , qsTr("Wallet successfully deleted.")
                                        , qsTr("Wallet Name: %1\nWallet ID: %2").arg(dlg.walletInfo.name).arg(walletId))
                }
            }
        })
        dlg.open()
    }

    function manageEncryptionDialog(walletId) {
        var dlg = Qt.createComponent("BsDialogs/WalletManageEncryptionDialog.qml").createObject(mainWindow)
        dlg.walletInfo = qmlFactory.createWalletInfo(walletId)
        dlg.open()
    }
}
