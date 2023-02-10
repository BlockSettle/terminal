/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.15

import "../BsStyles"
import "../StyledControls"
import "." as OverviewControls

Rectangle {
    id: control

    width: 1200
    height: 788
    color: "transparent"

    signal requestWalletProperties()
    signal createNewWallet()
    signal walletIndexChanged(index : int)
    signal openAddressDetails(var address, var transactions, var balance, var comment, var type, var wallet)

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

                Text {
                    text: qsTr("Addresses")
                    color: BSStyle.textColor
                    font.pixelSize: 19
                    font.family: "Roboto"
                    font.weight: Font.DemiBold
                    
                }

                CustomTableView {
                    width: parent.width
                    height: parent.height - 40

                    model: addressListModel
                    copy_button_column_index: 0

                    columnWidths: [0.35, 0.15, 0.1, 0.4]
                    onCopyRequested: bsApp.copyAddressToClipboard(id)
                    onCellClicked: (row, column, data) => {
                        const address = (column === 0) ? data : model.data(model.index(row, 0), 257)
                        const transactions = model.data(model.index(row, 1), 257)
                        const balance = model.data(model.index(row, 2), 257)
                        const comment = model.data(model.index(row, 3), 257)
                        const type = model.data(model.index(row, 0), 261)

                        openAddressDetails(address, transactions, balance, comment, type, overview_panel.currentWallet)
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
                    model: pendingTxListModel

                    copy_button_column_index: 3
                    columnWidths: [0.12, 0.1, 0.08, 0.3, 0.1, 0.1, 0.1, 0.1]
                    onCopyRequested: bsApp.copyAddressToClipboard(id)
                }
            }
        }
    }
}
