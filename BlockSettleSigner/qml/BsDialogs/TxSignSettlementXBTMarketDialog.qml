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
    readonly property string inputProduct: " XBT"
    readonly property string fxProduct: passwordDialogData.value("FxProduct")

    function getQuantity() {
        if (is_sell) {
            return txInfo.amount
        } else {
            return txInfo.amountCCReceived(product) * balanceDivider / lotSize
        }
    }

    Component.onCompleted: {
        quantity = getQuantity() + " " + product
        totalValue = (getQuantity() * price).toFixed(8) + " XBT"
        priceString = price + " XBT / 1 " + fxProduct
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

        // SettlementAddress
        CustomLabel {
            visible: passwordDialogData.contains("SettlementAddress")
            Layout.fillWidth: true
            text: qsTr("Settlement Address")
        }
        CustomLabelCopyableValue {
            id: settlementAddress
            visible: passwordDialogData.contains("SettlementAddress")
            text: passwordDialogData.value("SettlementAddress")
                .truncString(passwordDialogData.contains("RequesterAuthAddress") ? passwordDialogData.value("RequesterAuthAddress").length : 30)
            Layout.alignment: Qt.AlignRight
            textForCopy: passwordDialogData.value("SettlementAddress")

            ToolTip.text: passwordDialogData.value("SettlementAddress")
            ToolTip.delay: 150
            ToolTip.timeout: 5000
            ToolTip.visible: settlementAddress.mouseArea.containsMouse
        }

        // SettlementId
        CustomLabel {
            visible: passwordDialogData.contains("SettlementId")
            Layout.fillWidth: true
            text: qsTr("Settlement Id")
        }
        CustomLabelCopyableValue {
            id: settlementId
            visible: passwordDialogData.contains("SettlementId")
            text: passwordDialogData.value("SettlementId")
                .truncString(passwordDialogData.contains("RequesterAuthAddress") ? passwordDialogData.value("RequesterAuthAddress").length : 30)
            Layout.alignment: Qt.AlignRight
            textForCopy: passwordDialogData.value("SettlementId")

            ToolTip.text: passwordDialogData.value("SettlementId")
            ToolTip.delay: 150
            ToolTip.timeout: 5000
            ToolTip.visible: settlementId.mouseArea.containsMouse
        }

        // Requester Authentication Address
        CustomLabel {
            visible: passwordDialogData.contains("RequesterAuthAddress")
            Layout.fillWidth: true
            text: qsTr("Requester Auth")
        }
        CustomLabelCopyableValue {
            visible: passwordDialogData.contains("RequesterAuthAddress")
            text: passwordDialogData.value("RequesterAuthAddress")
            Layout.alignment: Qt.AlignRight
            color: passwordDialogData.requesterAuthAddressVerified ? BSStyle.inputsValidColor : BSStyle.inputsInvalidColor
        }

        // Responder Authentication Address = dealer
        CustomLabel {
            visible: passwordDialogData.contains("ResponderAuthAddress")
            Layout.fillWidth: true
            text: qsTr("Responder Auth")
        }
        CustomLabelCopyableValue {
            visible: passwordDialogData.contains("ResponderAuthAddress")
            text: passwordDialogData.value("ResponderAuthAddress")
            Layout.alignment: Qt.AlignRight
            color: passwordDialogData.responderAuthAddressVerified ? BSStyle.inputsValidColor : BSStyle.inputsInvalidColor
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

        // Input Amount
        CustomLabel {
            Layout.fillWidth: true
            text: qsTr("Input Amount")
        }
        CustomLabelValue {
            text: "- " + txInfo.inputAmount.toFixed(8) + inputProduct
            Layout.alignment: Qt.AlignRight
        }

        // Return Amount
        CustomLabel {
            Layout.fillWidth: true
            text: qsTr("Return Amount")
        }
        CustomLabelValue {
            text: "+ " + txInfo.changeAmount.toFixed(8) + inputProduct
            Layout.alignment: Qt.AlignRight
        }

        // Network Fee
        CustomLabel {
            Layout.fillWidth: true
            text: qsTr("Network Fee")
        }
        CustomLabelValue {
            text: "- " + txInfo.fee.toFixed(8) + inputProduct
            Layout.alignment: Qt.AlignRight
        }

        // Settlement Pay-In
        CustomLabel {
            visible: passwordDialogData.value("SettlementPayInVisible") === true
            Layout.fillWidth: true
            text: qsTr("Settlement Pay-In")
        }
        CustomLabelValue {
            visible: passwordDialogData.value("SettlementPayInVisible") === true
            text: "- " + txInfo.amount.toFixed(8) + inputProduct
            Layout.alignment: Qt.AlignRight
        }

        // Settlement Pay-Out
        CustomLabel {
            visible: passwordDialogData.value("SettlementPayOutVisible") === true
            Layout.fillWidth: true
            text: qsTr("Settlement Pay-Out")
        }
        CustomLabelValue {
            visible: passwordDialogData.value("SettlementPayOutVisible") === true
            text: "+ " + txInfo.amount.toFixed(8) + inputProduct
            Layout.alignment: Qt.AlignRight
        }

        // Total value
        CustomLabel {
            visible: passwordDialogData.value("TotalSpentVisible") === true
            Layout.fillWidth: true
            text: qsTr("Total Spent")
        }
        CustomLabelValue {
            visible: passwordDialogData.value("TotalSpentVisible") === true
            text: ((passwordDialogData.value("SettlementPayInVisible") === true) ? "- " : "+ ") + txInfo.total.toFixed(8) + inputProduct
            Layout.alignment: Qt.AlignRight
        }
     }
}
