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
    property string address

    signal requestPageChange(var text)

    Column {
        spacing: BSSizes.applyScale(20)
        anchors.leftMargin: BSSizes.applyScale(18)
        anchors.rightMargin: BSSizes.applyScale(18)
        anchors.bottomMargin: BSSizes.applyScale(18)
        anchors.fill: parent

        Row {
            spacing: BSSizes.applyScale(16)
            height: BSSizes.applyScale(20)

            Label {
                text: qsTr("Address")
                color: BSStyle.textColor
                font.pixelSize: BSSizes.applyScale(20)
                font.family: "Roboto"
                font.weight: Font.Bold
                font.letterSpacing: 0.35
                height: parent.height
                verticalAlignment: Text.AlignBottom
            }
            Label {
                id: address_label
                text: address !== null ? address : ""
                color: BSStyle.textColor
                font.pixelSize: BSSizes.applyScale(14)
                height: parent.height
                verticalAlignment: Text.AlignBottom
            
                CopyIconButton {
                    anchors.left: address_label.right
                    onCopy: bsApp.copyAddressToClipboard(address)
                }
            }
        }

        Rectangle {
            width: parent.width
            height: BSSizes.applyScale(60)
            anchors.bottomMargin: BSSizes.applyScale(24)
            anchors.topMargin: BSSizes.applyScale(18)
            anchors.leftMargin: BSSizes.applyScale(18)
            anchors.rightMargin: BSSizes.applyScale(18)

            radius: BSSizes.applyScale(14)
            color: BSStyle.addressesPanelBackgroundColor

            border.width: BSSizes.applyScale(1)
            border.color: BSStyle.comboBoxBorderColor

            Row {
                anchors.fill: parent
                anchors.verticalCenter: parent.verticalCenter

                BaseBalanceLabel {
                    width: BSSizes.applyScale(130)
                    label_text: qsTr("Transaction count")
                    label_value: txListByAddrModel.nbTx
                    anchors.verticalCenter: parent.verticalCenter
                }

                Rectangle {
                    width: BSSizes.applyScale(1)
                    height: BSSizes.applyScale(36)
                    color: BSStyle.tableSeparatorColor
                    anchors.verticalCenter: parent.verticalCenter
                }

                BaseBalanceLabel {
                    width: BSSizes.applyScale(130)
                    label_text: qsTr("Balance (BTC)")
                    label_value: txListByAddrModel.balance
                    anchors.verticalCenter: parent.verticalCenter
                }

                Rectangle {
                    width: BSSizes.applyScale(1)
                    height: BSSizes.applyScale(36)
                    color: BSStyle.tableSeparatorColor
                    anchors.verticalCenter: parent.verticalCenter
                }

                BaseBalanceLabel {
                    width: BSSizes.applyScale(150)
                    label_text: qsTr("Total Received (BTC)")
                    label_value: txListByAddrModel.totalReceived
                    anchors.verticalCenter: parent.verticalCenter
                }

                Rectangle {
                    width: BSSizes.applyScale(1)
                    height: BSSizes.applyScale(36)
                    color: BSStyle.tableSeparatorColor
                    anchors.verticalCenter: parent.verticalCenter
                }

                BaseBalanceLabel {
                    width: BSSizes.applyScale(130)
                    label_text: qsTr("Total Sent (BTC)")
                    label_value: txListByAddrModel.totalSent
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        Label {
            text: qsTr("Transactions")
            color: BSStyle.textColor
            font.pixelSize: BSSizes.applyScale(20)
            font.family: "Roboto"
            font.weight: Font.Bold
        }

        CustomTableView {
            width: parent.width
            height: parent.height - BSSizes.applyScale(150)
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
