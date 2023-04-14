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

Rectangle {
    id: transactions
    color: BSStyle.backgroundColor

    width: BSSizes.applyScale(1200)
    height: BSSizes.applyScale(788)

    signal openSend (string txId, bool isRBF, bool isCPFP)
    signal openExplorer (string txId)

    FileDialog  {
        id: fileDialogCSV
        visible: false
        title: qsTr("Choose CSV file name")
        folder: shortcuts.documents
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

    CustomFailDialog {
        id: fail_dialog
        visible: false;
    }

    CustomExportSuccessDialog{
        id: succes_dialog
        header: "Success"
        visible: false
    }

    Column {
        spacing: 18
        anchors.fill: parent
        anchors.topMargin: BSSizes.applyScale(14)
        anchors.leftMargin: BSSizes.applyScale(18)
        anchors.rightMargin: BSSizes.applyScale(18)

        Row {
            id: transaction_header_menu
            width: parent.width
            height: BSSizes.applyScale(45)
            spacing: BSSizes.applyScale(15)

            Label {
                text: qsTr("Transactions list")
                color: BSStyle.textColor
                font.pixelSize: BSSizes.applyScale(20)
                font.family: "Roboto"
                font.weight: Font.Bold
                font.letterSpacing: 0.35
            }

            Row 
            {
                spacing: BSSizes.applyScale(8)
                height: parent.height
                anchors.right: parent.right

                CustomSmallComboBox {
                    id: txWalletsComboBox
                    model: bsApp.txWalletsList

                    width: BSSizes.applyScale(124)
                    height: BSSizes.applyScale(29)

                    anchors.verticalCenter: parent.verticalCenter

                    onActivated: (index) => {
                        transactionFilterModel.walletName = index == 0 ? "" : txWalletsComboBox.currentValue
                        tableView.update()
                    }

                    Connections {
                        target: transactionFilterModel
                        onChanged: {
                            if (transactionFilterModel.walletName != txWalletsComboBox.currentValue) {
                                for (var i = 0; i < bsApp.txWalletsList.length; ++i) {
                                    if (bsApp.txWalletsList[i] == transactionFilterModel.walletName) {
                                        txWalletsComboBox.currentIndex = i
                                    }
                                }
                            }
                        }
                    }
                }

                CustomSmallComboBox {
                    id: txTypesComboBox
                    model: bsApp.txTypesList

                    width: BSSizes.applyScale(124)
                    height: BSSizes.applyScale(29)

                    anchors.verticalCenter: parent.verticalCenter

                    onActivated: (index) => {
                        transactionFilterModel.transactionType = index == 0 ? "" : txTypesComboBox.currentValue
                        tableView.update()
                    }

                    Connections {
                        target: transactionFilterModel
                        onChanged: {
                            if (transactionFilterModel.transactionType != txTypesComboBox.currentValue) {
                                for (var i = 0; i < bsApp.txTypesList.length; ++i) {
                                    if (bsApp.txTypesList[i] == transactionFilterModel.transactionType) {
                                        txTypesComboBox.currentIndex = i
                                    }
                                }
                            }
                        }
                    }
                }

                Row {
                    spacing: BSSizes.applyScale(4)
                    anchors.verticalCenter: parent.verticalCenter
                    
                    CustomButtonLeftIcon {
                        text: qsTr("From")

                        custom_icon.source: "qrc:/images/calendar_icon.svg"

                    }

                    Rectangle {
                        height: BSSizes.applyScale(1)
                        width: BSSizes.applyScale(8)
                        color: BSStyle.tableSeparatorColor
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    CustomButtonLeftIcon {
                        text: qsTr("To")

                        custom_icon.source: "qrc:/images/calendar_icon.svg"
                    }
                }

                CustomButtonRightIcon {
                    text: qsTr("CSV download")

                    custom_icon.source: "qrc:/images/download_icon.svg"
                    custom_icon.width: BSSizes.applyScale(10)
                    custom_icon.height: BSSizes.applyScale(10)

                    onClicked: {
                        var csvFile = "%1/BlockSettle_%2_%3_%4.csv"
                           .arg(fileDialogCSV.folder)
                           .arg(txWalletsComboBox.currentIndex === 0 ? "all" : txWalletsComboBox.currentText)
                           .arg(txListModel.getBegDate())
                           .arg(txListModel.getEndDate())

                        if (txListModel.exportCSVto(csvFile)) {
                            succes_dialog.path_name = csvFile.replace(/^(file:\/{3})/,"")
                            show_popup(succes_dialog)
                        }
                        else {
                            fail_dialog.header = "Export CSV Error"
                            fail_dialog.fail = "Failed to export CSV File"
                            show_popup(fail_dialog)
                        }
                    }
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        CustomTransactionsTableView {
            id: tableView
            width: parent.width
            height: parent.height - transaction_header_menu.height - transaction_header_menu.spacing - 1
            model: transactionFilterModel

            onOpenSend: (txId, isRBF, isCPFP) => control.openSend(txId, isRBF, isCPFP)
            onOpenExplorer: (txId) => transactions.openExplorer(txId)
        }

        function show_popup (id)
        {
            id.show()
            id.raise()
            id.requestActivate()
        }
    }
}
