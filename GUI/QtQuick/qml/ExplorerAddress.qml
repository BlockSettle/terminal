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
//import "BsControls"
//import "BsDialogs"
//import "js/helper.js" as JsHelper

Item {
    property var address

    Column {
        spacing: 23
        anchors.fill: parent

        Label {
            text: " "
            font.pointSize: 50
            height: 50
        }
        Row {
            spacing: 16
            Label {
                text: qsTr("<font color=\"white\">Address</font>")
                font.pointSize: 14
            }
            Label {
                text: address
                color: 'lightgrey'
                font.pointSize: 12
            }
            Button {
                text: qsTr("Copy")
                font.pointSize: 12
                onClicked: {
                    bsApp.copyAddressToClipboard(address)
                }
            }
        }
        Row {
            spacing: 12
            Column {
                Label {
                    text: qsTr("<font color=\"gray\">Transactions count</font>")
                    font.pointSize: 8
                }
                Label {
                    text: qsTr("<font color=\"white\">%1</font>").arg(txListByAddrModel.nbTx)
                    font.pointSize: 12
                }
            }
            Column {
                Label {
                    text: qsTr("<font color=\"gray\">Balance (BTC)</font>")
                    font.pointSize: 8
                }
                Label {
                    text: qsTr("<font color=\"white\">%1</font>").arg(txListByAddrModel.balance)
                    font.pointSize: 12
                }
            }
            Column {
                Label {
                    text: qsTr("<font color=\"gray\">Total Received (BTC)</font>")
                    font.pointSize: 8
                }
                Label {
                    text: qsTr("<font color=\"white\">%1</font>").arg(txListByAddrModel.totalReceived)
                    font.pointSize: 12
                }
            }
            Column {
                Label {
                    text: qsTr("<font color=\"gray\">Total Sent (BTC)</font>")
                    font.pointSize: 8
                }
                Label {
                    text: qsTr("<font color=\"white\">%1</font>").arg(txListByAddrModel.totalSent)
                    font.pointSize: 12
                }
            }
        }
        Label {
            text: qsTr("<font color=\"white\">Transactions</font>")
            font.pointSize: 14
        }
        TableView {
            width: 1008
            height: 450
            columnSpacing: 1
            rowSpacing: 1
            clip: true
            ScrollIndicator.horizontal: ScrollIndicator { }
            ScrollIndicator.vertical: ScrollIndicator { }
            model: txListByAddrModel
            delegate: Rectangle {
                implicitWidth: 112 * colWidth
                implicitHeight: 20
                border.color: "black"
                border.width: 1
                color: heading ? 'black' : 'darkslategrey'
                clip: true
                Text {
                    text: tableData
                    font.pointSize: heading ? 8 : 10
                    color: dataColor
                    anchors.centerIn: parent
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if (!heading && (model.column === 1)) {
                            explorerTX.txId = tableData
                            bsApp.startSearch(explorerTX.txId)
                            explorerStack.push(explorerTX)
                        }
                    }
                }
            }
        }
    }
}
