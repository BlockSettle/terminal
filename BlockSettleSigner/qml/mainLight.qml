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

    visible: false
    title: qsTr("BlockSettle Signer")
    width: 450
    height: 600
//    minimumWidth: 450
//    minimumHeight: 600
    onWidthChanged: {
        if (width > Screen.desktopAvailableWidth) {
            x = 0
            width = Screen.desktopAvailableWidth
        }
        emitSizeChanged()
    }
    onHeightChanged: {
        if (height > Screen.desktopAvailableHeight) {
            let frameSize = qmlFactory.frameSize(mainWindow)
            let h = frameSize.height > height ? frameSize.height - height : 0
            y = 0
            height = Screen.desktopAvailableHeight - h
        }
        emitSizeChanged()
    }

    property var currentDialog: ({})

    function emitSizeChanged() {
        sizeChangeTimer.start()
    }
    Timer {
        id: sizeChangeTimer
        interval: 5
        repeat: false
        running: false
        onTriggered: sizeChanged(mainWindow.width, mainWindow.height)
    }
    signal sizeChanged(int w, int h)

    Component.onCompleted: {
        mainWindow.flags = Qt.CustomizeWindowHint | Qt.MSWindowsFixedSizeDialogHint |
                Qt.Dialog | Qt.WindowSystemMenuHint |
                Qt.WindowTitleHint | Qt.WindowCloseButtonHint
        hide()
        qmlFactory.installEventFilterToObj(mainWindow)
    }

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

    InfoBanner {
        id: ibSuccess
        bgColor: "darkgreen"
    }
    InfoBanner {
        id: ibFailure
        bgColor: "darkred"
    }

    signal passwordEntered(string walletId, QPasswordData passwordData, bool cancelledByUser)

    function createTxSignDialog(prompt, txInfo, walletInfo) {
        // called from QMLAppObj::requestPassword

        currentDialog = Qt.createComponent("BsDialogs/TxSignDialog.qml").createObject(mainWindow)
        currentDialog.walletInfo = walletInfo
        currentDialog.prompt = prompt
        currentDialog.txInfo = txInfo

        show()

        currentDialog.sizeChanged.connect(function(w, h){
            mainWindow.width = w
            mainWindow.height = h
        })

        currentDialog.bsAccepted.connect(function() {
            passwordEntered(walletInfo.walletId, currentDialog.passwordData, false)
            currentDialog.destroy()
            hideWindow()
        })
        currentDialog.bsRejected.connect(function() {
            passwordEntered(walletInfo.walletId, currentDialog.passwordData, true)
            currentDialog.destroy()
            hideWindow()
        })

        mainWindow.width = currentDialog.width
        mainWindow.height = currentDialog.height + currentDialog.recvAddrHeight
        mainWindow.title = currentDialog.title
        if (typeof currentDialog.qmlTitleVisible !== "undefined") {
            currentDialog.qmlTitleVisible = false
        }

        currentDialog.dialogsChainFinished.connect(function(){ hide() })
        currentDialog.nextChainDialogChangedOverloaded.connect(function(nextDialog){
            mainWindow.width = nextDialog.width
            mainWindow.height = nextDialog.height
        })

        mainWindow.requestActivate()
        currentDialog.open()
        currentDialog.init()

        mainWindow.closing.connect(function() { currentDialog.bsRejected() });
    }

    function raiseWindow() {
        JsHelper.raiseWindow(mainWindow)
    }
    function hideWindow() {
        JsHelper.hideWindow(mainWindow)
    }

    function customDialogRequest(dialogName, data) {
        raiseWindow()
        var newDialog = JsHelper.customDialogRequest(dialogName, data)
        JsHelper.prepareLigthModeDialog(newDialog)
    }

    function invokeQmlMetod(method, cppCallback, argList) {
        raiseWindow()
        JsHelper.evalWorker(method, cppCallback, argList)
    }

    function moveMainWindowToScreenCenter() {
        mainWindow.x = Screen.virtualX + (Screen.width - mainWindow.width) / 2
        mainWindow.y = Screen.virtualY + (Screen.height - mainWindow.height) / 2
    }
}
