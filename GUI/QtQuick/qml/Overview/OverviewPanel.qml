/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.15
import QtQuick.Controls 2.15

import "../BsStyles"
import "../StyledControls"
import "." as OverviewControls
import terminal.models 1.0

Rectangle {
    id: control

    signal openSend (string txId, bool isRBF, bool isCPFP)

    width: 1200
    height: 788
    color: "transparent"

    signal requestWalletProperties()
    signal createNewWallet()
    signal walletIndexChanged(index : int)
    signal openAddressDetails(var address, var transactions, var balance, var comment, var asset_type, var type, var wallet)

    Column {
        anchors.leftMargin: 18
        anchors.rightMargin: 18
        anchors.fill: parent
        spacing: 0

        OverviewControls.OverviewWalletBar {
            id: overview_panel
            width: parent.width
            height: 100

            onRequestWalletProperties: control.requestWalletProperties()
            onCreateNewWallet: control.createNewWallet()
            onWalletIndexChanged: control.walletIndexChanged(index)
        }

        Rectangle {
            height: (parent.height - overview_panel.height) * 0.65
            width: parent.width
            anchors.horizontalCenter: parent.horizontalCenter

            radius: 16
            color: BSStyle.addressesPanelBackgroundColor
            border.width: 1
            border.color: BSStyle.tableSeparatorColor

            Column {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 10
                
                Rectangle {
                    id: tableMenu
                    color: "transparent"
                    width: parent.width
                    height: 30

                    Text {
                        text: qsTr("Addresses")
                        color: BSStyle.textColor
                        font.pixelSize: 19
                        font.family: "Roboto"
                        font.weight: Font.DemiBold
                    }

                    Row {
                        anchors.right: parent.right
                        spacing: 6

                        CustomSmallButton {
                            width: 85
                            text: qsTr("Hide used")
                        }

                        CustomSmallButton {
                            width: 90
                            text: qsTr("Hide internal")
                        }

                        CustomSmallButton {
                            width: 90
                            text: qsTr("Hide external")
                        }

                        CustomSmallButton {
                            width: 85
                            text: qsTr("Hide empty")
                        }
                    }
                }

                CustomTableView {
                    id: tablewView
                    width: parent.width
                    height: parent.height - tableMenu.height

                    model: addressListModel
                    copy_button_column_index: 0

                    columnWidths: [0.35, 0.15, 0.1, 0.4]
                    onCopyRequested: bsApp.copyAddressToClipboard(id)
                    onCellClicked: (row, column, data) => {
                        const address = (column === 0) ? data : model.data(model.index(row, 0), 257)
                        const transactions = model.data(model.index(row, 1), 257)
                        const balance = model.data(model.index(row, 2), 257)
                        const comment = model.data(model.index(row, 3), 257)
                        const type = model.data(model.index(row, 0), 259)
                        const asset_type = model.data(model.index(row, 0), 260)

                        openAddressDetails(address, transactions, balance, comment, asset_type, type, overview_panel.currentWallet)
                    }
                }
            }
        }

        Rectangle {
            color: "transparent"
            width: parent.width
            height: (parent.height - overview_panel.height) * 0.35

            Column {
                anchors.fill: parent
                anchors.topMargin: 20
                spacing: 10

                Text {
                    text: qsTr("Non-settled Transactions")
                    color: BSStyle.textColor
                    font.pixelSize: 19
                    font.family: "Roboto"
                    font.weight: Font.DemiBold
                    
                }

                CustomTableView {
                    width: parent.width
                    height: parent.height - 40
                    
                    model: PendingTransactionFilterModel {
                        sourceModel: txListModel
                        dynamicSortFilter: true
                    }

                    copy_button_column_index: 3
                    columnWidths: [0.12, 0.1, 0.08, 0.3, 0.1, 0.1, 0.1, 0.1]
                    onCopyRequested: bsApp.copyAddressToClipboard(id)

                    onCellRightClicked: (row, column, data) => {
                        context_menu.row = row
                        context_menu.column = column
                        context_menu.popup()
                    }

                    CustomRbfCpfpMenu {
                        id: context_menu

                        onOpenSend: (txId, isRBF, isCPFP) => control.openSend(txId, isRBF, isCPFP)
                    }
                }
            }
        }
    }
}
