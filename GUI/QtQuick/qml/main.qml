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
    minimumWidth: 1200
    minimumHeight: 900

    visible: false
    title: qsTr("BlockSettle Terminal")

    property var currentDialog: ({})
    readonly property int resizeAnimationDuration: 25

    Component.onCompleted: {
        hide()
//        qmlFactory.installEventFilterToObj(mainWindow)
//        qmlFactory.applyWindowFix(mainWindow)
    }

    CreateWallet {
        id: createWallet
        visible: false
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
        height: 57
        width: parent.width
        spacing: 0

        RowLayout {
            height: 56
            width: parent.width
            spacing: 0

            Image {
                width: 129
                height: 24
                source: "qrc:/images/logo.png"
                Layout.leftMargin : 18
            }

            Label {
                width: 13
            }

            Image {
                id: imgArmoryStatus
                source: (bsApp.armoryState === 7) ? "qrc:/images/conn_ind.png" : "qrc:/images/conn_inactive.png"
            }

            Label {
                Layout.fillWidth: true
            }

            CustomTitleToolButton {
                id: btnSend
                enabled: !bsApp.walletsList.empty
                text: qsTr("Send")
                icon.source: "qrc:/images/send_icon.png"
                font.pointSize: 16
                Layout.fillHeight: true
                onClicked: {
                    topMenuBtnClicked(btnSend)
                    stack.push(sendPage)
                }
            }
            CustomTitleToolButton {
                id: btnReceive
                text: qsTr("Receive")
                icon.source: "qrc:/images/receive_icon.png"

                font.pointSize: 16
                Layout.fillHeight: true
                enabled: !bsApp.walletsList.empty

                onClicked: {
                    topMenuBtnClicked(btnReceive)
                    stack.push(receivePage)
                }
            }
            CustomTitleToolButton {
                id: btnSettings
                text: qsTr("Settings")
                icon.source: "qrc:/images/settings_icon.png"
                onClicked: {
                    topMenuBtnClicked(btnSettings)
                    stack.push(settingsPage)
                }
            }
        }

        Rectangle {
            width: parent.width
            height: 1
            color: "#3C435A"
        }
    }

    SwipeView {
        anchors.fill: parent
        id: swipeView
        currentIndex: tabBar.currentIndex

        OverviewPage {
            id: overviewPage
            onNewWalletClicked: {
                createWallet.show()
                createWallet.raise()
                createWallet.requestActivate()
            }
        }

        TransactionsPage {
            id: transactionsPage
        }

        ExplorerPage {
            id: explorerPage
        }
    }

    footer: Rectangle {
        height: 56
        width: parent.width
        color :"#191E2A"

        RowLayout {
            spacing: 0
            anchors.fill: parent

            Label {
                Layout.fillWidth: true
            }

            TabBar {
                id: tabBar
                currentIndex: swipeView.currentIndex
                padding: 0
                spacing: 0
                Layout.fillWidth: false

                background: Rectangle {
                    color: "transparent"
                }

                CustomTabButton {
                    id: btnOverview
                    text: qsTr("Overview")
                    Component.onCompleted: {
                        btnOverview.setIcons ("qrc:/images/overview_icon.png", "qrc:/images/overview_icon_not_choosed.png")
                    }
                }
                CustomTabButton {
                    id: btnTransactions
                    text: qsTr("Transactions")
                    Component.onCompleted: {
                        btnTransactions.setIcons ("qrc:/images/transactions_icon.png", "qrc:/images/transactions_icon_unchoosed.png")
                    }
                }

                CustomTabButton {
                    id: btnExplorer
                    text: qsTr("Explorer")
                    Component.onCompleted: {
                        btnExplorer.setIcons ("qrc:/images/explorer_icon.png", "qrc:/images/explorer_icon_unchoosed.png")
                    }
                }

                CustomTabButton {
                    id: btnPlugins
                    text: qsTr("Plugins")
                    Component.onCompleted: {
                        btnPlugins.setIcons ("qrc:/images/plugins_icon.png", "qrc:/images/plugins_icon_unchoosed.png")
                    }
                }
            }

            Label {
                Layout.fillWidth: true
            }
        }
    }

    function topMenuBtnClicked(clickedBtn)
    {
        btnSend.select(false)
        btnReceive.select(false)
        btnSettings.select(false)

        clickedBtn.select(true)
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
