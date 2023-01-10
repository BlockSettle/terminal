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

        Row {
            spacing: 15

            Label {
                text: qsTr("<font color=\"white\">Transactions list (%1)</font>").arg(txListModel.nbTx)
                font.pointSize: 14
            }
            Item {  // spacer item
                Layout.fillWidth: true
                Layout.fillHeight: true
                Rectangle { anchors.fill: parent; color: "#ffaaaa" }
            }
            ComboBox {
                id: txWalletsComboBox
                objectName: "txWalletsComboBox"
                model: bsApp.txWalletsList
                font.pointSize: 8
            }
            ComboBox {
                id: txTypesComboBox
                objectName: "txTypesComboBox"
                model: bsApp.txTypesList
                font.pointSize: 8
            }
            Button {
                text: qsTr("From")
                font.pointSize: 8
            }
            Label {
                text: qsTr("<font color=\"darkgrey\">-</font>")
                font.pointSize: 12
            }
            Button {
                text: qsTr("To")
                font.pointSize: 8
            }
            TextEdit {
                id: txSearchBox
                width: 75
                height: 32
                Text {
                    text: qsTr("Search")
                    color: 'darkgrey'
                    visible: !txSearchBox.text && !txSearchBox.activeFocus
                }
            }
            Button {
                text: qsTr("CSV download")
                font.pointSize: 8
                onClicked: {
                    fileDialogCSV.visible = true
                }
            }
        }

        TableView {
            width: 1000
            height: 600
            columnSpacing: 1
            rowSpacing: 1
            clip: true
            ScrollIndicator.horizontal: ScrollIndicator { }
            ScrollIndicator.vertical: ScrollIndicator { }
            model: txListModel
            delegate: Rectangle {
                implicitWidth: 125 * colWidth
                implicitHeight: 20
                border.color: "black"
                border.width: 1
                clip: true
                color: heading ? 'black' : 'darkslategrey'
                Text {
                    text: tableData
                    font.pointSize: heading ? 8 : 10
                    color: dataColor
                    anchors.centerIn: parent
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if (!heading) {
                            bsApp.copyAddressToClipboard(txId)
                            ibInfo.displayMessage(qsTr("TX id %1 copied to clipboard").arg(txId))
                        }
                    }
                    onDoubleClicked: {
                        if (!heading) {
                            //TODO: show TX details
                        }
                    }
                }
            }
        }
    }
}
