/*

***********************************************************************************
* Copyright (C) 2022, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.15
import QtQuick.Controls 2.9
import QtQuick.Layouts 1.3
import QtQuick.Window 2
import Qt.labs.platform 1.1

import "../StyledControls" 1
import "../BsStyles" 1
import "../Receive"  1
import "../Send"  1
import "../CreateWallet"  1
import "../Pin"  1

ApplicationWindow {
    id: mainWindow
    minimumWidth: 1200
    minimumHeight: 900

    visible: false
    title: qsTr("BlockSettle Terminal")

    readonly property int resizeAnimationDuration: 25

    Component.onCompleted: {
        hide()
    }

    function reload(url)
    {
        loader.source = ""
        bsApp.clearCache()
        loader.source = "file:////home/yauhen/Workspace/terminal/GUI/QtQuick/qml/Debug/TerminalMainWindow.qml"
    }

    Loader {
        id: loader
        anchors.fill: parent
        onLoaded: bsApp.reconnectSignals()
    }

    Shortcut {
        sequence: "Ctrl+R"
        onActivated: {
            console.log('Reloaded')
            mainWindow.reload()
        }
    }

    color: BSStyle.backgroundColor

    overlay.modal: Rectangle {
        color: BSStyle.backgroundModalColor
    }
    overlay.modeless: Rectangle {
        color: BSStyle.backgroundModeLessColor
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

    //global functions
    function getWalletData (index: int, role: string)
    {
        return walletBalances.data(walletBalances.index(index, 0), role)
    }

    function getFeeSuggData (index: int, role: string)
    {
        return feeSuggestions.data(feeSuggestions.index(index, 0), role)
    }

    SystemTrayIcon {
        id: trayIcon
        visible: true
        icon.source: "qrc:/images/bs_logo.png"

        onActivated: {
            mainWindow.show()
            mainWindow.raise()
            mainWindow.requestActivate()
        }

        Component.onCompleted: {
            trayIcon.show()
        }
    }

    Connections {
        target: bsApp
        function onShowNotification(title, text) {
            trayIcon.showMessage(title, text, SystemTrayIcon.Information, 1000)
        }
    }
}
