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
import terminal.models 1.0

import "StyledControls"
import "BsStyles"
import "Overview"
//import "BsControls"
//import "BsDialogs"
//import "js/helper.js" as JsHelper

Item {
    property var tx: null

    signal requestPageChange(var text)

    Column {
        spacing: BSSizes.applyScale(23)
        anchors.leftMargin: BSSizes.applyScale(18)
        anchors.rightMargin: BSSizes.applyScale(18)
        anchors.bottomMargin: BSSizes.applyScale(18)
        anchors.fill: parent

        Row {
            spacing: BSSizes.applyScale(16)
            height: BSSizes.applyScale(20)
            width: parent.width

            Label {
                text: qsTr("Transaction ID")
                height: parent.height
                color: BSStyle.textColor
                font.pixelSize: BSSizes.applyScale(20)
                font.family: "Roboto"
                font.weight: Font.Bold
                font.letterSpacing: 0.35
                verticalAlignment: Text.AlignBottom
            }
            Label {
                id: transactionIdLabel
                height: parent.height
                text: tx ? tx.txId : qsTr("Unknown")
                color: BSStyle.textColor
                font.pixelSize: BSSizes.applyScale(14)
                font.family: "Roboto"
                verticalAlignment: Text.AlignBottom
            
                CopyIconButton {
                    anchors.left: transactionIdLabel.right
                    onCopy: bsApp.copyAddressToClipboard(tx.txId)
                }
            }
        }

        Rectangle {
            width: parent.width
            height: BSSizes.applyScale(60)
            anchors.bottomMargin: BSSizes.applyScale(24)
            anchors.topMargin: BSSizes.applyScale(24)
            anchors.leftMargin: BSSizes.applyScale(18)
            anchors.rightMargin: BSSizes.applyScale(18)

            radius: BSSizes.applyScale(14)
            color: BSStyle.addressesPanelBackgroundColor

            border.width: 1
            border.color: BSStyle.comboBoxBorderColor

            Row {
                anchors.fill: parent
                anchors.verticalCenter: parent.verticalCenter

                BaseBalanceLabel {
                    width: BSSizes.applyScale(110)
                    label_text: qsTr("Confirmations")
                    label_value: tx !== null ? tx.nbConf : ""
                    anchors.verticalCenter: parent.verticalCenter
                    label_value_color: "green"
                }

                Rectangle {
                    width: 1
                    height: BSSizes.applyScale(36)
                    color: BSStyle.tableSeparatorColor
                    anchors.verticalCenter: parent.verticalCenter
                }

                BaseBalanceLabel {
                    width: BSSizes.applyScale(80)
                    label_text: qsTr("Inputs")
                    label_value: tx !== null ? tx.nbInputs : ""
                    anchors.verticalCenter: parent.verticalCenter
                }

                Rectangle {
                    width: BSSizes.applyScale(1)
                    height: BSSizes.applyScale(36)
                    color: BSStyle.tableSeparatorColor
                    anchors.verticalCenter: parent.verticalCenter
                }

                BaseBalanceLabel {
                    width: BSSizes.applyScale(90)
                    label_text: qsTr("Outputs")
                    label_value: tx !== null ? tx.nbOutputs : ""
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
                    label_text: qsTr("Input Amount (BTC)")
                    label_value: tx !== null ? tx.inputAmount : ""
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
                    label_text: qsTr("Output Amount (BTC)")
                    label_value: tx !== null ? tx.outputAmount : ""
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
                    label_text: qsTr("Fees (BTC)")
                    label_value: tx !== null ? tx.fee : ""
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
                    label_text: qsTr("Fee per byte (s/b)")
                    label_value: tx !== null ? tx.feePerByte : ""
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
                    label_text: qsTr("Size (virtual bytes)")
                    label_value: tx !== null ? tx.virtSize : ""
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        Row {
            spacing: BSSizes.applyScale(24)
            width: parent.width
            height: parent.height - BSSizes.applyScale(120)

            Rectangle {
                height: parent.height
                width: parent.width / 2 - BSSizes.applyScale(12)

                color: "transparent"
                radius: BSSizes.applyScale(14)

                border.width: 1
                border.color: BSStyle.tableSeparatorColor

                Column {
                    anchors.fill: parent
                    anchors.margins: BSSizes.applyScale(20)

                    Row {
                        spacing: BSSizes.applyScale(11)
                        Label {
                            text: qsTr("Input")
                            color: BSStyle.textColor
                            font.pixelSize: BSSizes.applyScale(20)
                            font.family: "Roboto"
                            font.weight: Font.Bold
                        }
                        Image {
                            width: BSSizes.applyScale(9)
                            height: BSSizes.applyScale(12)
                            source: "qrc:/images/down_arrow.svg"
                            anchors.leftMargin: BSSizes.applyScale(20)
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    InputOutputTableView {
                        width: parent.width
                        height: parent.height - BSSizes.applyScale(20)
                        model: tx !== null ? tx.inputs : []
                        copy_button_column_index: -1
                        columnWidths: [0.0, 0.7, 0.2, 0.1]
                        onCopyRequested: bsApp.copyAddressToClipboard(id)

                        onCellClicked: (row, column, data, mouse) => {
                            var address = model.data(model.index(row, 1), TxInOutModel.TableDataRole)
                            requestPageChange(address)
                        }
                    }
                }
            }

            Rectangle {
                height: parent.height
                width: parent.width / 2 - BSSizes.applyScale(12)

                color: "transparent"
                radius: BSSizes.applyScale(14)

                border.width: BSSizes.applyScale(1)
                border.color: BSStyle.tableSeparatorColor

                Column {
                    anchors.fill: parent
                    anchors.margins: BSSizes.applyScale(20)

                    Row {
                        spacing: BSSizes.applyScale(11)
                        Label {
                            text: qsTr("Output")
                            color: BSStyle.textColor
                            font.pixelSize: BSSizes.applyScale(20)
                            font.family: "Roboto"
                            font.weight: Font.Bold
                        }
                        Image {
                            width: BSSizes.applyScale(9)
                            height: BSSizes.applyScale(12)
                            source: "qrc:/images/up_arrow.svg"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    InputOutputTableView {
                        width: parent.width
                        height: parent.height - BSSizes.applyScale(20)
                        model: tx !== null ? tx.outputs : []
                        copy_button_column_index: -1
                        columnWidths: [0.0, 0.7, 0.2, 0.1]
                        onCopyRequested: bsApp.copyAddressToClipboard(id)

                        onCellClicked: (row, column, data, mouse) => {
                            var address = model.data(model.index(row, 1), TxInOutModel.TableDataRole)
                            requestPageChange(address)
                        }
                    }
                }
            }
        }
    }
}
