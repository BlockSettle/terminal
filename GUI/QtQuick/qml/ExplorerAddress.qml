/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQml.Models 2

import "StyledControls"
import "BsStyles"
import "Overview"
//import "BsControls"
//import "BsDialogs"
//import "js/helper.js" as JsHelper

Item {
    property var address

    signal requestPageChange(var text)

    Column {
        spacing: 20
        anchors.leftMargin: 18
        anchors.rightMargin: 18
        anchors.bottomMargin: 18
        anchors.fill: parent

        Row {
            spacing: 16

            Label {
                text: qsTr("Address")
                color: BSStyle.textColor
                font.pixelSize: 20
                font.weight: Font.Bold
                anchors.bottom: parent.bottom
            }
            Label {
                id: address_label
                text: address
                color: BSStyle.textColor
                font.pixelSize: 14
                anchors.bottom: parent.bottom
            }
            
            CopyIconButton {
                anchors.left: address.right
                onCopy: bsApp.copyAddressToClipboard(address)
            }
        }

        Rectangle {
            width: parent.width
            height: 60
            anchors.bottomMargin: 24
            anchors.topMargin: 18
            anchors.leftMargin: 18
            anchors.rightMargin: 18

            radius: 14
            color: BSStyle.addressesPanelBackgroundColor

            border.width: 1
            border.color: BSStyle.comboBoxBorderColor

            Row {
                anchors.fill: parent
                anchors.verticalCenter: parent.verticalCenter

                BaseBalanceLabel {
                    width: 130
                    label_text: qsTr("Transaction count")
                    label_value: txListByAddrModel.nbTx
                    anchors.verticalCenter: parent.verticalCenter
                }

                Rectangle {
                    width: 1
                    height: 36
                    color: BSStyle.tableSeparatorColor
                    anchors.verticalCenter: parent.verticalCenter
                }

                BaseBalanceLabel {
                    width: 130
                    label_text: qsTr("Balance (BTC)")
                    label_value: txListByAddrModel.balance
                    anchors.verticalCenter: parent.verticalCenter
                }

                Rectangle {
                    width: 1
                    height: 36
                    color: BSStyle.tableSeparatorColor
                    anchors.verticalCenter: parent.verticalCenter
                }

                BaseBalanceLabel {
                    width: 150
                    label_text: qsTr("Total Received (BTC)")
                    label_value: txListByAddrModel.totalReceived
                    anchors.verticalCenter: parent.verticalCenter
                }

                Rectangle {
                    width: 1
                    height: 36
                    color: BSStyle.tableSeparatorColor
                    anchors.verticalCenter: parent.verticalCenter
                }

                BaseBalanceLabel {
                    width: 130
                    label_text: qsTr("Total Sent (BTC)")
                    label_value: txListByAddrModel.totalSent
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        Label {
            text: qsTr("Transactions")
            color: BSStyle.textColor
            font.pixelSize: 20
            font.weight: Font.Bold
        }

        CustomTableView {
            width: parent.width
            height: 500
            model: txListByAddrModel

            copy_button_column_index: 1
            columnWidths: [0.12, 0.46, 0.05, 0.04, 0.04, 0.08, 0.08, 0.07, 0.06]    
            onCopyRequested: bsApp.copyAddressToClipboard(id)

            // TODO: change constant 257 with C++ defined enum
            onCellClicked: (row, column, data) => {
                var tx_id = column === 1 ? data : model.data(model.index(row, 1), 257)
                requestPageChange(tx_id)
            }
        }
    }
}
