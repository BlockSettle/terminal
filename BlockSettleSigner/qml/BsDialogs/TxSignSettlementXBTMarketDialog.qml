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
    readonly property bool is_payOut: passwordDialogData.value("PayOutType") === true
    readonly property bool is_payIn: !is_payOut

    readonly property string onRevokeLabel: passwordDialogData.value("PayOutRevokeType") === true ? qsTr(" On Revoke") : ""

    function getTotalValue() {
        if (is_payOut) {
            return txInfo.amountXBTReceived()
        } else {
            return txInfo.total
        }
    }

    Component.onCompleted: {
        if (passwordDialogData.contains("Quantity")) {
            quantity = passwordDialogData.value("Quantity")
        }
        if (passwordDialogData.contains("TotalValue")) {
            totalValue = passwordDialogData.value("TotalValue")
        }
        priceString = price + " " + fxProduct + " / 1 XBT"
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
            visible: is_payIn
            Layout.fillWidth: true
            text: qsTr("Input Amount")
        }
        CustomLabelValue {
            visible: is_payIn
            text: "- " + txInfo.inputAmount.toFixed(8) + inputProduct
            Layout.alignment: Qt.AlignRight
        }

        // Return Amount
        CustomLabel {
            visible: is_payIn
            Layout.fillWidth: true
            text: qsTr("Return Amount")
        }
        CustomLabelValue {
            visible: is_payIn
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
            visible: is_payIn
            Layout.fillWidth: true
            text: qsTr("Settlement Pay-In")
        }
        CustomLabelValue {
            visible: is_payIn
            text: "- " + txInfo.amount.toFixed(8) + inputProduct
            Layout.alignment: Qt.AlignRight
        }

        // Sent amount
        CustomLabel {
            visible: is_payOut
            Layout.fillWidth: true
            text: qsTr("Sent Amount")
        }
        CustomLabelValue {
            visible: is_payOut
            text: "+ " + (txInfo.amountXBTReceived() + txInfo.fee).toFixed(8) + inputProduct
            Layout.alignment: Qt.AlignRight
        }

        // Settlement Pay-Out
        CustomLabel {
            visible: is_payOut
            Layout.fillWidth: true
            text: qsTr("Settlement Pay-Out")
        }
        CustomLabelValue {
            visible: is_payOut
            text: "+ " + txInfo.amount.toFixed(8) + inputProduct
            Layout.alignment: Qt.AlignRight
        }

        // Total value
        CustomLabel {
            visible: passwordDialogData.value("TotalSpentVisible") === true
            Layout.fillWidth: true
            text: is_payIn ? qsTr("Total Sent ") : qsTr("Total Received ") + onRevokeLabel
        }
        CustomLabelValue {
            visible: passwordDialogData.value("TotalSpentVisible") === true
            text: (is_payIn ? "- " : "+ ") + getTotalValue().toFixed(8) + inputProduct
            Layout.alignment: Qt.AlignRight
        }
     }
}
