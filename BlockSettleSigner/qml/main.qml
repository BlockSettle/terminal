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

        dlg.bsAccepted.connect(function() {
            passwordEntered(walletInfo.walletId, dlg.passwordData, false)
        })
        dlg.bsRejected.connect(function() {
            passwordEntered(walletInfo.walletId, dlg.passwordData, true)
        })
        mainWindow.requestActivate()
        dlg.open()

        dlg.init()
    }
    function raiseWindow() {
        JsHelper.raiseWindow(mainWindow)
    }
    function hideWindow() {
        JsHelper.hideWindow(mainWindow)
    }

    function customDialogRequest(dialogName, data) {
        JsHelper.customDialogRequest(dialogName, data)
    }

    function showError(text) {
        ibFailure.displayMessage(text)
    }

    function getJsCallback(reqId) {
        return function(argList){ qmlFactory.execJsCallback(reqId, argList)}
    }

    function test(jsCallback, prompt, txInfo, walletInfo) {
        // called from QMLAppObj::requestPassword

        var dlg = Qt.createComponent("BsDialogs/TxSignDialog.qml").createObject(mainWindow)
        dlg.walletInfo = walletInfo
        dlg.prompt = prompt
        dlg.txInfo = txInfo

        dlg.bsAccepted.connect(function() {
            jsCallback(walletInfo.walletId, dlg.passwordData, false)
        })
        dlg.bsRejected.connect(function() {
            jsCallback(walletInfo.walletId, dlg.passwordData, true)
        })
        mainWindow.requestActivate()
        dlg.open()

        dlg.init()
    }

    function invokeQmlMetod(method, cb, val0, val1, val2, val3, val4, val5, val6, val7) {
        eval(method)(cb, val0, val1, val2, val3, val4, val5, val6, val7)
    }

    function terminalHandshakeFailed(peerAddress) {
        JsHelper.messageBoxCritical("Authentication failure", "An incoming connection from address " + peerAddress + " has failed to authenticate themselves. Please ensure that you have imported the Terminal ID Key from those Terminals you wish to have access to your wallets.")
    }

    function moveMainWindowToScreenCenter() {
        /*mainWindow.x = (Screen.width - mainWindow.width) / 2
        mainWindow.y = (Screen.height - mainWindow.height) / 2*/
    }

}
