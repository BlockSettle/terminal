/*

***********************************************************************************
* Copyright (C) 2022, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2
import QtQuick.Controls 2.9
import QtQuick.Layouts 1.3
import QtQuick.Window 2
import "StyledControls" 1
import "BsStyles" 1
//import Qt.labs.settings 1.0

/*
import "BsControls"
import "BsDialogs"
import "js/helper.js" as JsHelper
*/

ApplicationWindow {
    id: mainWindow
    minimumWidth: 1024
    minimumHeight: 800

    visible: false
    title: qsTr("BlockSettle Terminal")

    property var currentDialog: ({})
    readonly property int resizeAnimationDuration: 25

    Component.onCompleted: {
        mainWindow.flags = Qt.CustomizeWindowHint | Qt.MSWindowsFixedSizeDialogHint |
                Qt.Dialog | Qt.WindowSystemMenuHint |
                Qt.WindowTitleHint | Qt.WindowCloseButtonHint
        hide()
//        qmlFactory.installEventFilterToObj(mainWindow)
//        qmlFactory.applyWindowFix(mainWindow)
    }

    color: BSStyle.backgroundColor

    overlay.modal: Rectangle {
        color: BSStyle.backgroundModalColor
    }
    overlay.modeless: Rectangle {
        color: BSStyle.backgroundModeLessColor
    }

    // attached to use from c++
/*    function messageBoxCritical(title, text, details) {
        return JsHelper.messageBoxCritical(title, text, details)
    }*/

    InfoBanner {
        id: ibSuccess
        bgColor: "darkgreen"
    }
    InfoBanner {
        id: ibFailure
        bgColor: "darkred"
    }
    InfoBanner {
        id: ibInfo
        bgColor: "darkgrey"
    }

    StackView {
        id: stack
        initialItem: swipeView
        anchors.fill: parent
    }

    SendPage {
        id: sendPage
        visible: false
    }

    ReceivePage {
        id: receivePage
        visible: false
    }

    SettingsPage {
        id: settingsPage
        visible: false
    }

    header: Column {
        height: 50
        width: parent.width

        RowLayout {
            height: 50
            width: parent.width

            Image {
                source: "qrc:/images/bs_logo.png"
                horizontalAlignment: Qt.AlignLeft
                verticalAlignment: Qt.AlignTop
                Layout.fillHeight: true
            }

            Image {
                id: imgArmoryStatus
                source: "qrc:/images/bitcoin-disabled.png"
                verticalAlignment: Qt.AlignVCenter
            }

            Label {
                Layout.fillWidth: true
            }

            ToolButton {
                id: btnSend
                text: qsTr("Send")
                icon.source: "qrc:/images/send_icon.png"
                font.pointSize: 16
                Layout.fillHeight: true
                enabled: !bsApp.walletsList.empty
                onClicked: {
                    stack.push(sendPage)
                }
            }
            ToolButton {
                id: btnReceive
                text: qsTr("Receive")
                icon.source: "qrc:/images/receive_icon.png"
                font.pointSize: 16
                Layout.fillHeight: true
                enabled: !bsApp.walletsList.empty
                onClicked: {
                    stack.push(receivePage)
                }
            }
            ToolButton {
                id: btnSettings
                text: qsTr("Settings")
                icon.source: "qrc:/images/settings_icon.png"
                font.pointSize: 16
                Layout.fillHeight: true
                onClicked: {
                    stack.push(settingsPage)
                }
            }
        }

        Rectangle {
            width: parent.width
            height: 1
        }
    }

    SwipeView {
        anchors.fill: parent
        id: swipeView
        currentIndex: tabBar.currentIndex
        Layout.fillWidth: true
        Layout.fillHeight: true

        OverviewPage {
            id: overviewPage
        }

        TransactionsPage {
            id: transactionsPage
        }

        ExplorerPage {
            id: explorerPage
        }
    }

    footer: TabBar {
        id: tabBar
        currentIndex: swipeView.currentIndex
        spacing: 5
        Layout.fillWidth: false
        background: Rectangle {
            color: "transparent"
        }

        CustomTabButton {
            id: btnOverview
            text: qsTr("Overview")
            icon.source: "qrc:/images/overview_icon.png"
        }
        CustomTabButton {
            id: btnTransactions
            text: qsTr("Transactions")
            icon.source: "qrc:/images/transactions_icon.png"
        }

        CustomTabButton {
            id: btnExplorer
            text: qsTr("Explorer")
            icon.source: "qrc:/images/explorer_icon.png"
        }
    }


/*    function raiseWindow() {
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
    }*/

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
