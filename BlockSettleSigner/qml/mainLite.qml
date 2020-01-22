/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
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
    readonly property bool isLiteMode: true

    visible: false
    title: qsTr("BlockSettle Signer")

    property var currentDialog: ({})
    readonly property int resizeAnimationDuration: 25

    Component.onCompleted: {
        mainWindow.flags = Qt.CustomizeWindowHint | Qt.MSWindowsFixedSizeDialogHint |
                Qt.Dialog | Qt.WindowSystemMenuHint |
                Qt.WindowTitleHint | Qt.WindowCloseButtonHint
        hide()
        qmlFactory.installEventFilterToObj(mainWindow)
        qmlFactory.applyWindowFix(mainWindow)
        JsHelper.initJSDialogs()
    }

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

    InfoBanner {
        id: ibSuccess
        bgColor: "darkgreen"
    }
    InfoBanner {
        id: ibFailure
        bgColor: "darkred"
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

    function invokeQmlMethod(method, cppCallback, argList) {
        JsHelper.evalWorker(method, cppCallback, argList)
    }

    function moveMainWindowToScreenCenter() {
        mainWindow.x = Screen.virtualX + (Screen.width - mainWindow.width) / 2
        mainWindow.y = Screen.virtualY + (Screen.height - mainWindow.height) / 2
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
}
