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

import ".."
import "../StyledControls" 1
import "../BsStyles" 1
import "../Receive"  1
import "../Send"  1
import "../CreateWallet"  1
import "../Pin"  1

Item {
    id: item

    property var currentDialog: ({})
    property int overviewWalletIndex

    CreateWallet {
        id: create_wallet
        visible: false
    }

    ReceivePopup {
        id: receive_popup
        visible: false
        onClosing: {
            btnReceive.select(false)
        }
    }

    SendPopup {
        id: send_popup
        visible: false
        onClosing: {
            btnSend.select(false)
        }
    }

    PinEntriesPopup {
        id: pin_popup
        visible: false
    }

    Connections
    {
        target:bsApp
        function onInvokePINentry ()
        {
            pin_popup.init()
            pin_popup.show()
            pin_popup.raise()
            pin_popup.requestActivate()
        }
    }

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

    Column {
        anchors.fill: parent

        Rectangle {
            id: header
            height: 57
            width: parent.width
            color: "transparent"

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
                        bsApp.requestFeeSuggestions()
                        topMenuBtnClicked(btnSend)
                        //stack.push(sendPage)
                        send_popup.init()
                        send_popup.show()
                        send_popup.raise()
                        send_popup.requestActivate()
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
                        //stack.push(receivePage)
                        bsApp.generateNewAddress(overviewWalletIndex, true)
                        receive_popup.show()
                        receive_popup.raise()
                        receive_popup.requestActivate()
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
                anchors.bottom: parent.bottom
                color: "#3C435A"
            }
        }

        Rectangle {
            id: main_panel
            width: parent.width
            height: parent.height - header.height - footer.height
            color: "transparent"

        StackView {
            id: stack
            initialItem: swipeView
            anchors.fill: parent
        }

        SwipeView {
            id: swipeView
            currentIndex: tabBar.currentIndex

            OverviewPage {
                id: overviewPage
                width: main_panel.width
                height: main_panel.height

                onNewWalletClicked: {
                    create_wallet.init()
                    create_wallet.show()
                    create_wallet.raise()
                    create_wallet.requestActivate()
                }

                onCurWalletIndexChanged: (ind) => {
                    if (overviewWalletIndex != ind) {
                        overviewWalletIndex = ind
                    }
                }
            }

            TransactionsPage {
                id: transactionsPage
            }

            ExplorerPage {
                id: explorerPage
            }
        }
        }

        Rectangle {
            id: footer
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
    }

    function topMenuBtnClicked(clickedBtn)
    {
        btnSend.select(false)
        btnReceive.select(false)
        btnSettings.select(false)

        clickedBtn.select(true)
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
}
