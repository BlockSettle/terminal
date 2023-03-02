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
import ".."
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

    AddressFilterModel {
        id: addressFilterModel
        sourceModel: addressListModel
    }

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
                            onClicked: addressFilterModel.hideUsed = !addressFilterModel.hideUsed
                            backgroundColor: addressFilterModel.hideUsed ? BSStyle.smallButtonBackgroundColor : 'transparent'
                        }

                        CustomSmallButton {
                            width: 90
                            text: qsTr("Hide internal")
                            onClicked: addressFilterModel.hideInternal = !addressFilterModel.hideInternal
                            backgroundColor: addressFilterModel.hideInternal ? BSStyle.smallButtonBackgroundColor : 'transparent'
                        }

                        CustomSmallButton {
                            width: 90
                            text: qsTr("Hide external")
                            onClicked: addressFilterModel.hideExternal = !addressFilterModel.hideExternal
                            backgroundColor: addressFilterModel.hideExternal ? BSStyle.smallButtonBackgroundColor : 'transparent'
                        }

                        CustomSmallButton {
                            width: 85
                            text: qsTr("Hide empty")
                            onClicked: addressFilterModel.hideEmpty = !addressFilterModel.hideEmpty
                            backgroundColor: addressFilterModel.hideEmpty ? BSStyle.smallButtonBackgroundColor :  'transparent'
                        }
                    }
                }

                CustomTableView {
                    id: tablewView
                    width: parent.width
                    height: parent.height - tableMenu.height

                    model: addressFilterModel
                    copy_button_column_index: 0

                    columnWidths: [0.35, 0.15, 0.1, 0.4]
                    onCopyRequested: bsApp.copyAddressToClipboard(id)
                    onCellClicked: (row, column, data) => {
                        const address = (column === 0) ? data : model.data(model.index(row, 0), QmlAddressListModel.TableDataRole)
                        const transactions = model.data(model.index(row, 1), QmlAddressListModel.TableDataRole)
                        const balance = model.data(model.index(row, 2), QmlAddressListModel.TableDataRole)
                        const comment = model.data(model.index(row, 3), QmlAddressListModel.TableDataRole)
                        const type = model.data(model.index(row, 0), QmlAddressListModel.AddressTypeRole)
                        const asset_type = model.data(model.index(row, 0), QmlAddressListModel.AssetTypeRole)

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

                    onCellClicked: (row, column, data) => {
                        const txHash = model.data(model.index(row, 0), TxListModel.TxIdRole)
                        transactionDetails.walletName = model.data(model.index(row, 1), TxListModel.TableDataRole)
                        transactionDetails.address = model.data(model.index(row, 3), TxListModel.TableDataRole)
                        transactionDetails.txDateTime = model.data(model.index(row, 0), TxListModel.TableDataRole)
                        transactionDetails.txType = model.data(model.index(row, 2), TxListModel.TableDataRole)
                        transactionDetails.txTypeColor = model.data(model.index(row, 2), TxListModel.ColorRole)
                        transactionDetails.txComment = model.data(model.index(row, 7), TxListModel.TableDataRole)
                        transactionDetails.txAmount = model.data(model.index(row, 4), TxListModel.TableDataRole)
                        transactionDetails.txConfirmations = model.data(model.index(row, 5), TxListModel.TableDataRole)
                        transactionDetails.txConfirmationsColor = model.data(model.index(row, 5), TxListModel.ColorRole)
                        transactionDetails.tx = bsApp.getTXDetails(txHash)
                        transactionDetails.open()
                    }


                    TransactionDetails {
                        id: transactionDetails
                        visible: false
                    }
                }
            }
        }
    }
}
