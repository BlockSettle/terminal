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
import QtQuick.Dialogs 1.3

import "StyledControls"
import "BsStyles"

import terminal.models 1.0

Item {
    id: transactions

    width: 1200
    height: 788

    signal openSend (string txId, bool isRBF, bool isCPFP)

    TransactionFilterModel {
        id: transactionModel
        sourceModel: txListModel
    }

    TransactionDetails {
        id: transactionDetails
        visible: false
    }

    FileDialog  {
        id: fileDialogCSV
        visible: false
        title: qsTr("Choose CSV file name")
        folder: shortcuts.home
        defaultSuffix: "csv"
        selectExisting: false
        onAccepted: {
            var csvFile = fileUrl.toString()
            if (txListModel.exportCSVto(csvFile)) {
                ibInfo.displayMessage(qsTr("TX list CSV saved to %1").arg(csvFile))
            }
            else {
                ibFailure.displayMessage(qsTr("Failed to save CSV to %1").arg(csvFile))
            }
        }
    }

    Column {
        spacing: 18
        anchors.fill: parent
        anchors.topMargin: 14
        anchors.leftMargin: 18
        anchors.rightMargin: 18

        Row {
            id: transaction_header_menu
            width: parent.width
            height: 45
            spacing: 15

            Label {
                text: qsTr("Transactions list")
                font.pixelSize: 19
                font.weight: Font.DemiBold
                color: BSStyle.textColor
            }

            Row 
            {
                spacing: 8
                height: parent.height

                CustomSmallComboBox {
                    id: txWalletsComboBox
                    model: bsApp.txWalletsList
                    font.pointSize: 8

                    width: 124
                    height: 29

                    anchors.verticalCenter: parent.verticalCenter

                    onActivated: (index) => {
                        transactionModel.walletName = index == 0 ? "" : txWalletsComboBox.currentValue
                        tableView.update()
                    }
                }

                CustomSmallComboBox {
                    id: txTypesComboBox
                    model: bsApp.txTypesList
                    font.pointSize: 8

                    width: 124
                    height: 29

                    anchors.verticalCenter: parent.verticalCenter

                    onActivated: (index) => {
                        transactionModel.transactionType = index == 0 ? "" : txTypesComboBox.currentValue
                        tableView.update()
                    }
                }


                Row {
                    spacing: 4
                    anchors.verticalCenter: parent.verticalCenter
                    
                    CustomButtonLeftIcon {
                        text: qsTr("From")
                        font.pointSize: 8

                        custom_icon.source: "qrc:/images/calendar_icon.svg"

                    }

                    Rectangle {
                        height: 1
                        width: 8
                        color: BSStyle.tableSeparatorColor
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    CustomButtonLeftIcon {
                        text: qsTr("To")
                        font.pointSize: 8

                        custom_icon.source: "qrc:/images/calendar_icon.svg"
                    }
                }

                CustomButtonRightIcon {
                    text: qsTr("CSV download")
                    font.pointSize: 8

                    custom_icon.source: "qrc:/images/download_icon.svg"
                    custom_icon.width: 10
                    custom_icon.height: 10

                    onClicked: {
                        fileDialogCSV.visible = true
                    }
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        CustomTransactionsTableView {
            id: tableView
            width: parent.width
            height: parent.height - transaction_header_menu.height - transaction_header_menu.spacing - 1
            model: transactionModel

            onOpenSend: (txId, isRBF, isCPFP) => control.openSend(txId, isRBF, isCPFP)
        }
    }
}
