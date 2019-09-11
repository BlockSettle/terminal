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

CustomTitleDialogWindow {
    property WalletInfo walletInfo: WalletInfo{}
    property TXInfo txInfo: TXInfo {}
    property PasswordDialogData passwordDialogData: PasswordDialogData {}
    property QPasswordData passwordData: QPasswordData{}
    property AuthSignWalletObject  authSign: AuthSignWalletObject{}

    readonly property bool acceptable: walletInfo.encType === QPasswordData.Password ? tfPassword.text : true
    readonly property int addressRowHeight: 24
    readonly property int recipientsAddrHeight: txInfo.recipients.length < 4 ? txInfo.recipients.length * addressRowHeight : addressRowHeight * 3

    readonly property int duration: passwordDialogData.value("Duration") / 1000.0 - 1 > 0 ? passwordDialogData.value("Duration") / 1000.0 - 1 : 60

    property string inputProduct: " " + passwordDialogData.value("TxInputProduct")
    id: root
    title: passwordDialogData.value("Title")
    rejectable: true
    width: 500

    function clickConfirmBtn() {
        btnConfirm.clicked()
    }

    function init() {
        if (walletInfo.encType !== QPasswordData.Auth) {
            return
        }

        btnConfirm.visible = false
        btnCancel.anchors.horizontalCenter = barFooter.horizontalCenter

        authSign = qmlFactory.createAutheIDSignObject(AutheIDClient.SignWallet, walletInfo)

        authSign.succeeded.connect(function(encKey, password) {
            passwordData.encType = QPasswordData.Auth
            passwordData.encKey = encKey
            passwordData.binaryPassword = password
            acceptAnimated()
        });
        authSign.failed.connect(function(errorText) {
            var mb = JsHelper.messageBox(BSMessageBox.Type.Critical
                , qsTr("Wallet"), qsTr("eID request failed with error: \n") + errorText
                , qsTr("Wallet Name: %1\nWallet ID: %2").arg(walletInfo.name).arg(walletInfo.rootId))
            mb.bsAccepted.connect(function() { rejectAnimated() })
        })
        authSign.userCancelled.connect(function() {
            rejectAnimated()
        })
    }

    Connections {
        target: qmlAppObj

        onCancelSignTx: {
            console.log("SettlementTransactionDialog.qml, cancel requested for id=" + settlementId)
            if (settlementId === passwordDialogData.value("SettlementId")) {
                rejectAnimated()
            }
        }
    }

    onBsRejected: {
        if (authSign) {
            authSign.cancel()
        }
    }

    cContentItem: ColumnLayout {
        spacing: 10
        Layout.alignment: Qt.AlignTop

        GridLayout {
            id: gridRfqDetails
            columns: 2
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            rowSpacing: 0

            CustomHeader {
                Layout.fillWidth: true
                Layout.columnSpan: 2
                text: qsTr("RFQ Details")
                Layout.preferredHeight: 25
            }

            // Product Group
            CustomLabel {
                Layout.fillWidth: true
                text: qsTr("Product Group")
            }
            CustomLabelValue {
                text: passwordDialogData.value("ProductGroup")
                Layout.alignment: Qt.AlignRight
            }

            // Security ID
            CustomLabel {
                Layout.fillWidth: true
                text: qsTr("Security ID")
            }
            CustomLabelValue {
                text: passwordDialogData.value("Security")
                Layout.alignment: Qt.AlignRight
            }

            // Product
            CustomLabel {
                Layout.fillWidth: true
                text: qsTr("Product")
            }
            CustomLabelValue {
                text: passwordDialogData.value("Product")
                Layout.alignment: Qt.AlignRight
            }

            // Side
            CustomLabel {
                Layout.fillWidth: true
                text: qsTr("Side")
            }
            CustomLabelValue {
                text: passwordDialogData.value("Side")
                Layout.alignment: Qt.AlignRight
            }

            // Quantity
            CustomLabel {
                Layout.fillWidth: true
                text: qsTr("Quantity")
            }
            CustomLabelValue {
                text: passwordDialogData.value("Quantity")
                Layout.alignment: Qt.AlignRight
            }

            // Price
            CustomLabel {
                Layout.fillWidth: true
                text: qsTr("Price")
            }
            CustomLabelValue {
                text: passwordDialogData.value("Price")
                Layout.alignment: Qt.AlignRight
            }

            // Total Value
            CustomLabel {
                Layout.fillWidth: true
                text: qsTr("Total Value")
            }
            CustomLabelValue {
                text: passwordDialogData.value("TotalValue")
                Layout.alignment: Qt.AlignRight
            }
        }

        GridLayout {
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

            // Payment UTXO(s)
            RowLayout {
                Layout.columnSpan: 2
                Layout.fillWidth: true
                visible: passwordDialogData.contains("InputsListVisible")

                CustomLabel {
                    text: qsTr("Payment UTXO(s)")
                    Layout.alignment: Qt.AlignTop
                }

                ListView {
                    id: inputs
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignRight
                    model: txInfo.inputs
                    clip: true
                    Layout.preferredHeight: txInfo.inputs.length < 4 ? txInfo.inputs.length * addressRowHeight : addressRowHeight * 3

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
                visible: passwordDialogData.contains("RecipientsListVisible")

                CustomLabel {
                    text: qsTr("Delivery UTXO(s)")
                    Layout.alignment: Qt.AlignTop
                }

                ListView {
                    id: recipients
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignRight
                    model: txInfo.recipients
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
                            color: passwordDialogData.deliveryUTXOVerified ? BSStyle.inputsValidColor : BSStyle.inputsInvalidColor
                        }
                    }
                }
            }

        }

        GridLayout {
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
                visible: passwordDialogData.value("InputAmountVisible") === true
                Layout.fillWidth: true
                text: qsTr("Input Amount")
            }
            CustomLabelValue {
                visible: passwordDialogData.value("InputAmountVisible") === true
                text: "- " + txInfo.inputAmount.toFixed(8) + inputProduct
                Layout.alignment: Qt.AlignRight
            }

            // Return Amount
            CustomLabel {
                visible: passwordDialogData.value("ReturnAmountVisible") === true
                Layout.fillWidth: true
                text: qsTr("Return Amount")
            }
            CustomLabelValue {
                visible: passwordDialogData.value("ReturnAmountVisible") === true
                text: "+ " + txInfo.changeAmount.toFixed(8) + inputProduct
                Layout.alignment: Qt.AlignRight
            }

            // Network Fee
            CustomLabel {
                visible: passwordDialogData.value("NetworkFeeVisible") === true
                Layout.fillWidth: true
                text: qsTr("Network Fee")
            }
            CustomLabelValue {
                visible: passwordDialogData.value("NetworkFeeVisible") === true
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
                visible: passwordDialogData.value("TxTotalAmountVisible") === true
                Layout.fillWidth: true
                text: qsTr("Total TX Amount")
            }
            CustomLabelValue {
                visible: passwordDialogData.value("TxTotalAmountVisible") === true
                text: ((passwordDialogData.value("SettlementPayInVisible") === true) ? "- " : "+ ") + txInfo.total.toFixed(8) + inputProduct
                Layout.alignment: Qt.AlignRight
            }

            /// CC fields
            // Delivery Amount
            CustomLabel {
                visible: passwordDialogData.contains("DeliveryAmount")
                Layout.fillWidth: true
                text: qsTr("Delivery Amount")
            }
            CustomLabelValue {
                visible: passwordDialogData.contains("DeliveryAmount")
                text: passwordDialogData.value("DeliveryAmount")
                Layout.alignment: Qt.AlignRight
            }

            // Payment Received
            CustomLabel {
                visible: passwordDialogData.contains("PaymentReceived")
                Layout.fillWidth: true
                text: qsTr("Payment Received")
            }
            CustomLabelValue {
                visible: passwordDialogData.contains("PaymentReceived")
                text: passwordDialogData.value("PaymentReceived")
                Layout.alignment: Qt.AlignRight
            }

            // Delivery Amount
            CustomLabel {
                visible: passwordDialogData.contains("PaymentAmount")
                Layout.fillWidth: true
                text: qsTr("Payment Amount")
            }
            CustomLabelValue {
                visible: passwordDialogData.contains("PaymentAmount")
                text: passwordDialogData.value("PaymentAmount")
                Layout.alignment: Qt.AlignRight
            }

            // Payment Received
            CustomLabel {
                visible: passwordDialogData.contains("DeliveryReceived")
                Layout.fillWidth: true
                text: qsTr("Delivery Received")
            }
            CustomLabelValue {
                visible: passwordDialogData.contains("DeliveryReceived")
                text: passwordDialogData.value("DeliveryReceived")
                Layout.alignment: Qt.AlignRight
            }

        }

        RowLayout {
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomHeader {
                Layout.fillWidth: true
                text: qsTr("Decrypt Wallet")
                Layout.preferredHeight: 25
            }
        }

        RowLayout {
            spacing: 25
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomLabel {
                visible: walletInfo.encType === QPasswordData.Password
                Layout.minimumWidth: 110
                Layout.preferredWidth: 110
                Layout.maximumWidth: 110
                Layout.fillWidth: true
                text: qsTr("Password")
            }

            CustomPasswordTextInput {
                id: tfPassword
                visible: walletInfo.encType === QPasswordData.Password
                focus: true
                //placeholderText: qsTr("Password")
                Layout.fillWidth: true
                Keys.onEnterPressed: {
                    if (btnConfirm.enabled) btnConfirm.onClicked()
                }
                Keys.onReturnPressed: {
                    if (btnConfirm.enabled) btnConfirm.onClicked()
                }
            }

            CustomLabel {
                id: labelAuth
                visible: walletInfo.encType === QPasswordData.Auth
                text: authSign.status
            }
        }

        ColumnLayout {
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            Timer {
                id: timer
                property real timeLeft: duration
                interval: 500
                running: true
                repeat: true
                onTriggered: {
                    timeLeft -= 0.5
                    if (timeLeft <= 0) {
                        stop()
                        // assume non signed tx is cancelled tx
                        rejectAnimated()
                    }
                }
                signal expired()
            }

//            CustomLabel {
//                text: qsTr("On completion just press [Enter] or [Return]")
//                visible: walletInfo.encType !== QPasswordData.Auth
//                Layout.fillWidth: true
//            }
            CustomLabelValue {
                text: qsTr("%1 seconds left").arg(timer.timeLeft.toFixed(0))
                Layout.fillWidth: true
            }

            CustomProgressBar {
                Layout.minimumHeight: 6
                Layout.preferredHeight: 6
                Layout.maximumHeight: 6
                Layout.bottomMargin: 10
                Layout.fillWidth: true
                to: duration
                value: timer.timeLeft
            }
        }
    }

    cFooterItem: RowLayout {
        CustomButtonBar {
            id: barFooter
            Layout.fillWidth: true

            CustomButton {
                id: btnCancel
                text: qsTr("Cancel")
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                onClicked: {
                    rejectAnimated()
                }
            }

            CustomButtonPrimary {
                id: btnConfirm
                text: walletInfo.encType === QPasswordData.Password ? qsTr("CONFIRM") : qsTr("Continue")
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                enabled: (tfPassword.text.length || acceptable) && passwordDialogData.value("SigningAllowed") === true
                onClicked: {
                    if (walletInfo.encType === QPasswordData.Password) {
                        passwordData.textPassword = tfPassword.text
                        passwordData.encType = QPasswordData.Password
                        acceptAnimated()
                    }
                    else if (walletInfo.encType === QPasswordData.Auth) {
                    }
                    else {
                        passwordData.encType = QPasswordData.Unencrypted
                        acceptAnimated()
                    }
                }
            }
        }
    }
}
