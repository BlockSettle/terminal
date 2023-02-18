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
    property var tx

    signal requestPageChange(var text)

    Column {
        spacing: 23
        anchors.leftMargin: 18
        anchors.rightMargin: 18
        anchors.bottomMargin: 18
        anchors.fill: parent

        Row {
            spacing: 16

            Label {
                text: qsTr("Transaction ID")
                color: BSStyle.textColor
                font.pixelSize: 20
                font.weight: Font.Bold
                anchors.bottom: parent.bottom
            }
            Label {
                text: tx ? tx.txId : qsTr("Unknown")
                color: BSStyle.textColor
                font.pixelSize: 14
                anchors.bottom: parent.bottom
            }
            
            CopyIconButton {
                anchors.left: address.right
                onCopy: bsApp.copyAddressToClipboard(tx.txId)
            }
        }

        Rectangle {
            width: parent.width
            height: 60
            anchors.bottomMargin: 24
            anchors.topMargin: 24
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
                    width: 110
                    label_text: qsTr("Confirmations")
                    label_value: tx.nbConf
                    anchors.verticalCenter: parent.verticalCenter
                    label_value_color: "green"
                }

                Rectangle {
                    width: 1
                    height: 36
                    color: BSStyle.tableSeparatorColor
                    anchors.verticalCenter: parent.verticalCenter
                }

                BaseBalanceLabel {
                    width: 80
                    label_text: qsTr("Inputs")
                    label_value: tx.nbInputs
                    anchors.verticalCenter: parent.verticalCenter
                }

                Rectangle {
                    width: 1
                    height: 36
                    color: BSStyle.tableSeparatorColor
                    anchors.verticalCenter: parent.verticalCenter
                }

                BaseBalanceLabel {
                    width: 90
                    label_text: qsTr("Outputs")
                    label_value: tx.nbOutputs
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
                    label_text: qsTr("Input Amount (BTC)")
                    label_value: tx.inputAmount
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
                    label_text: qsTr("Output Amount (BTC)")
                    label_value: tx.outputAmount
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
                    label_text: qsTr("Fees (BTC)")
                    label_value: tx.fee
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
                    label_text: qsTr("Fee per byte (s/b)")
                    label_value: tx.feePerByte
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
                    label_text: qsTr("Size (virtual bytes)")
                    label_value: tx.virtSize
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        Row {
            spacing: 24
            width: parent.width
            height: 500

            Rectangle {
                height: parent.height
                width: parent.width / 2 - 12

                color: "transparent"
                radius: 14

                border.width: 1
                border.color: BSStyle.tableSeparatorColor

                Column {
                    anchors.fill: parent
                    anchors.margins: 20

                    Row {
                        spacing: 11
                        Label {
                            text: qsTr("Input")
                            color: BSStyle.textColor
                            font.pixelSize: 20
                            font.weight: Font.Bold
                        }
                        Image {
                            width: 9
                            height: 12
                            source: "qrc:/images/down_arrow.svg"
                            anchors.leftMargin: 20
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    InputOutputTableView {
                        width: parent.width
                        height: parent.height - 20
                        model: tx.inputs
                        copy_button_column_index: -1
                        columnWidths: [0.0, 0.7, 0.2, 0.1]
                        onCopyRequested: bsApp.copyAddressToClipboard(id)

                        // TODO: change constant 259 with C++ defined enum
                        onCellClicked: (row, column, data) => {
                            var address = model.data(model.index(row, 0), 259)
                            requestPageChange(address)
                        }
                    }
                }
            }

            Rectangle {
                height: parent.height
                width: parent.width / 2 - 12

                color: "transparent"
                radius: 14

                border.width: 1
                border.color: BSStyle.tableSeparatorColor

                Column {
                    anchors.fill: parent
                    anchors.margins: 20

                    Row {
                        spacing: 11
                        Label {
                            text: qsTr("Output")
                            color: BSStyle.textColor
                            font.pixelSize: 20
                            font.weight: Font.Bold
                        }
                        Image {
                            width: 9
                            height: 12
                            source: "qrc:/images/up_arrow.svg"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    InputOutputTableView {
                        width: parent.width
                        height: parent.height - 20
                        model: tx.outputs
                        copy_button_column_index: -1
                        columnWidths: [0.0, 0.7, 0.2, 0.1]
                        onCopyRequested: bsApp.copyAddressToClipboard(id)

                        // TODO: change constant 257 with C++ defined enum
                        onCellClicked: (row, column, data) => {
                            var address = model.data(model.index(row, 1), 257)
                            requestPageChange(address)
                        }
                    }
                }
            }
        }
    }
}
