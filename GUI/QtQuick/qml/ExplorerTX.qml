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
    property var tx
    property var expAddress

    Component.onCompleted: {
        expAddress = Qt.createComponent("ExplorerAddress.qml")
        expAddress.visible = false
    }

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
                text: qsTr("<font color=\"white\">Transaction ID</font>")
                font.pointSize: 14
            }
            Label {
                text: tx ? tx.txId : qsTr("Unknown")
                color: 'lightgrey'
                font.pointSize: 12
            }
            Button {
                text: qsTr("Copy")
                font.pointSize: 12
                onClicked: {
                    bsApp.copyAddressToClipboard(tx.txId)
                }
            }
        }
        Row {
            spacing: 12
            Column {
                Label {
                    text: qsTr("<font color=\"gray\">Confirmations</font>")
                    font.pointSize: 8
                }
                Label {
                    text: qsTr("<font color=\"green\">%1</font>").arg(tx.nbConf)
                    font.pointSize: 12
                }
            }
            Column {
                Label {
                    text: qsTr("<font color=\"gray\">Inputs</font>")
                    font.pointSize: 8
                }
                Label {
                    text: qsTr("<font color=\"white\">%1</font>").arg(tx.nbInputs)
                    font.pointSize: 12
                }
            }
            Column {
                Label {
                    text: qsTr("<font color=\"gray\">Outputs</font>")
                    font.pointSize: 8
                }
                Label {
                    text: qsTr("<font color=\"white\">%1</font>").arg(tx.nbOutputs)
                    font.pointSize: 12
                }
            }
            Column {
                Label {
                    text: qsTr("<font color=\"gray\">Input Amount (BTC)</font>")
                    font.pointSize: 8
                }
                Label {
                    text: qsTr("<font color=\"white\">%1</font>").arg(tx.inputAmount)
                    font.pointSize: 12
                }
            }
            Column {
                Label {
                    text: qsTr("<font color=\"gray\">Output Amount (BTC)</font>")
                    font.pointSize: 8
                }
                Label {
                    text: qsTr("<font color=\"white\">%1</font>").arg(tx.outputAmount)
                    font.pointSize: 12
                }
            }
            Column {
                Label {
                    text: qsTr("<font color=\"gray\">Fees (BTC)</font>")
                    font.pointSize: 8
                }
                Label {
                    text: qsTr("<font color=\"white\">%1</font>").arg(tx.fee)
                    font.pointSize: 12
                }
            }
            Column {
                Label {
                    text: qsTr("<font color=\"gray\">Fee per byte (s/b)</font>")
                    font.pointSize: 8
                }
                Label {
                    text: qsTr("<font color=\"white\">%1</font>").arg(tx.feePerByte)
                    font.pointSize: 12
                }
            }
            Column {
                Label {
                    text: qsTr("<font color=\"gray\">Size (virtual bytes)</font>")
                    font.pointSize: 8
                }
                Label {
                    text: qsTr("<font color=\"white\">%1</font>").arg(tx.virtSize)
                    font.pointSize: 12
                }
            }
        }
        Row {
            spacing: 32
            Column {
                Label {
                    text: qsTr("<font color=\"white\">Input</font>")
                    font.pointSize: 14
                }

                TableView {
                    width: 500
                    height: 300
                    columnSpacing: 1
                    rowSpacing: 1
                    clip: true
                    ScrollIndicator.horizontal: ScrollIndicator { }
                    ScrollIndicator.vertical: ScrollIndicator { }
                    model: tx ? tx.inputs : addressListModel
                    delegate: Rectangle {
                        implicitWidth: 125 * colWidth
                        implicitHeight: 20
                        border.color: "black"
                        border.width: 1
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
                                if (!heading && (model.column === 1)) {
                                    expAddress.address = address
                                    bsApp.startSearch(address)
                                    explorerStack.push(expAddress)
                                }
                            }
                        }
                    }
                }
            }
            Column {
                Label {
                    text: qsTr("<font color=\"white\">Output</font>")
                    font.pointSize: 14
                }
                TableView {
                    width: 500
                    height: 300
                    columnSpacing: 1
                    rowSpacing: 1
                    clip: true
                    ScrollIndicator.horizontal: ScrollIndicator { }
                    ScrollIndicator.vertical: ScrollIndicator { }
                    model: tx ? tx.outputs : addressListModel
                    delegate: Rectangle {
                        implicitWidth: 125 * colWidth
                        implicitHeight: 20
                        border.color: "black"
                        border.width: 1
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
                                if (!heading && (model.column === 1)) {
                                    visible = false
                                    expAddress.address = address
                                    bsApp.startSearch(address)
                                    explorerStack.push(expAddress)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
