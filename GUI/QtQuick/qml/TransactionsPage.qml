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
//import "BsControls"
//import "BsDialogs"
//import "js/helper.js" as JsHelper

Item {
    id: transactions

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
        spacing: 23
        anchors.fill: parent
        anchors.margins: 18

        Row {
            id: transaction_header_menu
            width: parent.width
            height: 45
            spacing: 15

            Label {
                text: qsTr("Transactions list")
                font.pixelSize: 20
                font.weight: Font.Bold
                color: BSStyle.textColor

                anchors.verticalCenter: parent.verticalCenter
            }

            Row 
            {
                spacing: 8
                height: parent.height
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter

                CustomSmallComboBox {
                    id: txWalletsComboBox
                    objectName: "txWalletsComboBox"
                    model: bsApp.txWalletsList
                    font.pointSize: 8

                    width: 124
                    height: 29

                    anchors.verticalCenter: parent.verticalCenter
                }

                CustomSmallComboBox {
                    id: txTypesComboBox
                    objectName: "txTypesComboBox"
                    model: bsApp.txTypesList
                    font.pointSize: 8

                    width: 124
                    height: 29

                    anchors.verticalCenter: parent.verticalCenter
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

        CustomTableView {
            width: parent.width
            height: parent.height - transaction_header_menu.height
            model: txListModel

            copy_button_column_index: 3
            columnWidths: [0.12, 0.1, 0.08, 0.3, 0.1, 0.1, 0.1, 0.1]    
            onCopyRequested: bsApp.copyAddressToClipboard(id)
        }
    }
}
