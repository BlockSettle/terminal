import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.4

import com.blocksettle.TXInfo 1.0
import com.blocksettle.PasswordDialogData 1.0
import com.blocksettle.AutheIDClient 1.0
import com.blocksettle.AuthSignWalletObject 1.0
import com.blocksettle.WalletInfo 1.0
import com.blocksettle.QSeed 1.0
import com.blocksettle.QPasswordData 1.0

import "../StyledControls"
import "../BsControls"
import "../BsStyles"
import "../js/helper.js" as JsHelper

TxSignSettlementBaseDialog {
    id: root

    readonly property string inputProduct: is_sell ? " " + passwordDialogData.TxInputProduct : " XBT"
    readonly property int lotSize: passwordDialogData.LotSize

    readonly property int recipientsAddrHeight: txInfo.counterPartyRecipients.length < 4 ? txInfo.counterPartyRecipients.length * addressRowHeight : addressRowHeight * 3
    readonly property int inputsXBTAddrHeight: txInfo.inputsXBT.length < 4 ? txInfo.inputsXBT.length * addressRowHeight : addressRowHeight * 3
    readonly property int inputsCCAddrHeight: txInfo.inputsCC.length < 4 ? txInfo.inputsCC.length * addressRowHeight : addressRowHeight * 3

    function displayAmount(amount) {
        if (is_sell) {
            return Math.round((amount * balanceDivider / lotSize))
        } else {
            return amount.toFixed(8)
        }
    }

    function getQuantity() {
        if (is_sell) {
            return Math.round(txInfo.amountCCSent() * balanceDivider / lotSize)
        } else {
            return Math.round(txInfo.amountCCReceived(product) * balanceDivider / lotSize)
        }
    }

    readonly property string inputAmount: minus_string + displayAmount(txInfo.inputAmount) + inputProduct
    readonly property string changeAmount: plus_string + displayAmount(txInfo.changeAmount) + inputProduct
    readonly property string fee: minus_string + displayAmount(txInfo.fee) + inputProduct

    signingAllowed: true
    errorMessage: qsTr("Genesis Address could not be verified")

    Component.onCompleted: {
        quantity = getQuantity() + " " + product
        totalValue = (getQuantity() * price).toFixed(8) + " XBT"
        priceString = price + " XBT / 1 " + product
    }

    settlementDetailsItem: GridLayout {
        id: gridSettlementDetails
        columns: 2
        Layout.leftMargin: 10
        Layout.rightMargin: 10
        rowSpacing: 0

        CustomHeader {
            Layout.fillWidth: true
            Layout.columnSpan: 2
            text: qsTr("Settlement Details")
            Layout.preferredHeight: 25
        }

        // SettlementId
        CustomLabel {
            visible: passwordDialogData.hasSettlementId()
            Layout.fillWidth: true
            text: qsTr("Settlement Id")
        }
        CustomLabelCopyableValue {
            id: settlementId
            visible: passwordDialogData.hasSettlementId()
            text: passwordDialogData.SettlementId
                .truncString(passwordDialogData.hasRequesterAuthAddress() ? passwordDialogData.RequesterAuthAddress.length : 30)
            Layout.alignment: Qt.AlignRight
            textForCopy: passwordDialogData.SettlementId

            ToolTip.text: passwordDialogData.SettlementId
            ToolTip.delay: 150
            ToolTip.timeout: 5000
            ToolTip.visible: settlementId.mouseArea.containsMouse
        }

        // Payment UTXO(s)
        RowLayout {
            Layout.columnSpan: 2
            Layout.fillWidth: true
            visible: passwordDialogData.InputsListVisible

            CustomLabel {
                text: qsTr("Payment Address(es)")
                Layout.alignment: Qt.AlignTop
            }

            ListView {
                id: inputs
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignRight
                model: is_sell ? txInfo.inputsCC : txInfo.inputsXBT
                clip: true
                Layout.preferredHeight: is_sell ? inputsCCAddrHeight : inputsXBTAddrHeight

                flickableDirection: Flickable.VerticalFlick
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {
                    active: true
                }

                delegate: Rectangle {
                    id: inputsAddressRect
                    color: "transparent"
                    height: 22
                    width: inputs.width

                    CustomLabelValue {
                        text: modelData
                        anchors.fill: inputsAddressRect
                        horizontalAlignment: Text.AlignRight
                        verticalAlignment: Text.AlignTop
                        font: fixedFont
                    }
                }
            }
        }

        // Delivery UTXO(s)
        RowLayout {
            Layout.columnSpan: 2
            Layout.fillWidth: true
            visible: passwordDialogData.RecipientsListVisible

            CustomLabel {
                text: qsTr("Delivery Address(es)")
                Layout.alignment: Qt.AlignTop
            }

            ListView {
                id: recipients
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignRight
                model: txInfo.counterPartyRecipients
                clip: true
                Layout.preferredHeight: recipientsAddrHeight

                flickableDirection: Flickable.VerticalFlick
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {
                    active: true
                }

                delegate: Rectangle {
                    id: recipientsAddressRect
                    color: "transparent"
                    height: 22
                    width: recipients.width

                    CustomLabelValue {
                        id: labelTxWalletId
                        text: modelData
                        anchors.fill: recipientsAddressRect
                        horizontalAlignment: Text.AlignRight
                        verticalAlignment: Text.AlignTop
                        font: fixedFont
                        color: passwordDialogData.DeliveryUTXOVerified ? BSStyle.inputsValidColor : BSStyle.inputsInvalidColor
                    }
                }
            }
        }
    }

    txDetailsItem: GridLayout {
        id: gridTxDetails
        columns: 2
        Layout.leftMargin: 10
        Layout.rightMargin: 10
        rowSpacing: 0

        CustomHeader {
            Layout.fillWidth: true
            Layout.columnSpan: 2
            text: qsTr("Transaction Details")
            Layout.preferredHeight: 25
        }

//        // Lot Size
//        CustomLabel {
//            Layout.fillWidth: true
//            text: qsTr("Lot Size")
//        }
//        CustomLabelValue {
//            text: lotSize
//            Layout.alignment: Qt.AlignRight
//        }

        // Input Amount
        CustomLabel {
            Layout.fillWidth: true
            text: qsTr("Input Amount")
        }
        CustomLabelValue {
            text: inputAmount
            Layout.alignment: Qt.AlignRight
        }

        // Return Amount
        CustomLabel {
            Layout.fillWidth: true
            text: qsTr("Return Amount")
        }
        CustomLabelValue {
            text: changeAmount
            Layout.alignment: Qt.AlignRight
        }

        // Network Fee
        CustomLabel {
            visible: is_buy
            Layout.fillWidth: true
            text: qsTr("Network Fee")
        }
        CustomLabelValue {
            visible: is_buy
            text: fee
            Layout.alignment: Qt.AlignRight
        }

        /// CC Sell
        // Delivery Amount
        CustomLabel {
            visible: is_sell
            Layout.fillWidth: true
            text: qsTr("Delivery Amount")
        }
        CustomLabelValue {
            visible: is_sell
            text: minus_string + displayAmount(txInfo.amount) + inputProduct
            Layout.alignment: Qt.AlignRight
        }

        // Payment Received
        CustomLabel {
            visible: is_sell
            Layout.fillWidth: true
            text: qsTr("Total Received")
        }
        CustomLabelValue {
            visible: is_sell
            text: plus_string + txInfo.amountXBTReceived().toFixed(8) + " XBT"
            Layout.alignment: Qt.AlignRight
        }

        /// CC Buy
        // Payment Amount
        CustomLabel {
            visible: is_buy
            Layout.fillWidth: true
            text: qsTr("Payment Amount")
        }
        CustomLabelValue {
            visible: is_buy
            text: minus_string + txInfo.amount.toFixed(8) + inputProduct
            Layout.alignment: Qt.AlignRight
        }

        // Delivery Received
        CustomLabel {
            visible: is_buy
            Layout.fillWidth: true
            text: qsTr("Delivery Received")
        }
        CustomLabelValue {
            visible: is_buy
            text: plus_string + getQuantity() + " " + product
            Layout.alignment: Qt.AlignRight
        }


        // Total value
        CustomLabel {
            visible: is_buy
            Layout.fillWidth: true
            text: qsTr("Total Sent")
        }
        CustomLabelValue {
            visible: is_buy
            text: minus_string + txInfo.total.toFixed(8) + inputProduct
            Layout.alignment: Qt.AlignRight
        }
    }
}
