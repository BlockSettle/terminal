/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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

    readonly property bool isLiteMode: false
    visible: true
    title: qsTr("BlockSettle Signer")

    property var currentDialog: ({})
    readonly property int resizeAnimationDuration: 25

    width: 800
    height: 600
    minimumWidth: 800
    minimumHeight: 600

    color: BSStyle.backgroundColor

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

    ColumnLayout {
        anchors.fill: parent

        InfoBar {
            id: infoBar
            Layout.fillWidth: true
            Layout.rightMargin: 5
            Layout.leftMargin: 5
        }

        SwipeView {
            id: swipeView
            currentIndex: tabBar.currentIndex
            Layout.fillWidth: true
            Layout.fillHeight: true

            StatusPage {
                id: dashboardPage
            }

            SettingsPage {
                id: settingsPage
                onSettingsChanged: infoBar.showChangeApplyMessage = true;
            }

            AutoSignPage {
                id: autoSignPage
            }

            WalletsPage {
                id: walletsPage
            }
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

    function resizeAnimated(w,h) {
        mwWidthAnimation.from = mainWindow.width
        mwWidthAnimation.to = w
        mwWidthAnimation.restart()

        mwHeightAnimation.from = mainWindow.height
        mwHeightAnimation.to = h
        mwHeightAnimation.restart()

        mwXAnimation.from = mainWindow.x
        mwXAnimation.to = Screen.virtualX + (Screen.width - w) / 2
        mwXAnimation.restart()

        mwYAnimation.from = mainWindow.y
        mwYAnimation.to = Screen.virtualY + (Screen.height - h) / 2
        mwYAnimation.restart()
    }

    NumberAnimation {
        id: mwWidthAnimation
        target: mainWindow
        property: "width"
        duration: resizeAnimationDuration
    }
    NumberAnimation {
        id: mwHeightAnimation
        target: mainWindow
        property: "height"
        duration: resizeAnimationDuration
    }

    NumberAnimation {
        id: mwXAnimation
        target: mainWindow
        property: "x"
        duration: resizeAnimationDuration
    }
    NumberAnimation {
        id: mwYAnimation
        target: mainWindow
        property: "y"
        duration: resizeAnimationDuration
    }

    onClosing: {
        let count = signerStatus.connectedClients.length
        if (count > 0) {
            close.accepted = false;
            var prompt = count === 1 ?
                        qsTr("Do you want to close signer?\nThere is %n connected terminal.", "", count) :
                        qsTr("Do you want to close signer?\nThere are %n connected terminals.", "", count);
            var mb = JsHelper.messageBox(BSMessageBox.Type.Question
                , qsTr("Close"), prompt)
            mb.bsAccepted.connect(function() {
                settingsPage.storeSettings();
                autoSignPage.storeSettings();
                Qt.quit()
            })
        }
    }

    function raiseWindow() {
        JsHelper.raiseWindow(mainWindow)
    }
    function hideWindow() {
        JsHelper.hideWindow(mainWindow)
    }

    function customDialogRequest(dialogName, data) {
        var newDialog = JsHelper.customDialogRequest(dialogName, data)
        if (newDialog) {
            raiseWindow()
            JsHelper.prepareDialog(newDialog)
        }
    }

    function showError(text) {
        ibFailure.displayMessage(text)
    }

    function getJsCallback(reqId) {
        return function(argList){ qmlFactory.execJsCallback(reqId, argList)}
    }

    function terminalHandshakeFailed(peerAddress) {
        JsHelper.messageBoxCritical("Authentication failure", "An incoming connection from address " + peerAddress + " has failed to authenticate themselves. Please ensure that you have imported the Terminal ID Key from those Terminals you wish to have access to your wallets.")
    }

    function invokeQmlMethod(method, cppCallback, argList) {
        JsHelper.evalWorker(method, cppCallback, argList)
    }
}
