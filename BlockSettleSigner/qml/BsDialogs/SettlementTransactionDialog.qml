import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2

import com.blocksettle.TXInfo 1.0
import com.blocksettle.PasswordDialogData 1.0
import com.blocksettle.AutheIDClient 1.0
import com.blocksettle.AuthSignWalletObject 1.0
import com.blocksettle.WalletInfo 1.0
import com.blocksettle.QSeed 1.0
import com.blocksettle.QPasswordData 1.0

import "../StyledControls"
import "../BsControls"
import "../js/helper.js" as JsHelper

CustomTitleDialogWindow {
    property string prompt
    property WalletInfo walletInfo: WalletInfo{}
    property TXInfo txInfo: TXInfo {}
    property PasswordDialogData passwordDialogData: PasswordDialogData {}
    property QPasswordData passwordData: QPasswordData{}
    property AuthSignWalletObject  authSign: AuthSignWalletObject{}

    property bool   acceptable: walletInfo.encType === QPasswordData.Password ? tfPassword.text : true
    property int addressRowHeight: 24
    //property int recvAddrHeight: txInfo.recvAddresses.length < 4 ? txInfo.recvAddresses.length * addressRowHeight : addressRowHeight * 3
    property int recvAddrHeight: 22

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
            if (txId === txInfo.txId) {
                rejectAnimated()
            }
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
                text: qsTr("Total Value (XBT)")
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
                visible: passwordDialogData.contains("Payment")
                           || passwordDialogData.contains("GenesisAddress")
                           || passwordDialogData.contains("RequesterAuthAddress")
                           || passwordDialogData.contains("ResponderAuthAddress")

                Layout.fillWidth: true
                Layout.columnSpan: 2
                text: qsTr("Settlement Details")
                Layout.preferredHeight: 25
            }

            // Payment
            CustomLabel {
                visible: passwordDialogData.contains("Payment")
                Layout.fillWidth: true
                text: qsTr("Payment")
            }
            CustomLabelValue {
                visible: passwordDialogData.contains("Payment")
                text: passwordDialogData.value("Payment")
                Layout.alignment: Qt.AlignRight
            }

            // Genesis Address
            CustomLabel {
                visible: passwordDialogData.contains("GenesisAddress")
                Layout.fillWidth: true
                text: qsTr("Genesis Address")
            }
            CustomLabelValue {
                visible: passwordDialogData.contains("GenesisAddress")
                text: passwordDialogData.value("GenesisAddress")
                Layout.alignment: Qt.AlignRight
            }

            // Requester Authentication Address
            CustomLabel {
                visible: passwordDialogData.contains("RequesterAuthAddress")
                Layout.fillWidth: true
                text: qsTr("Requester\nAuthentication Address")
            }
            CustomLabelValue {
                visible: passwordDialogData.contains("RequesterAuthAddress")
                text: passwordDialogData.value("RequesterAuthAddress")
                Layout.alignment: Qt.AlignRight
            }

            // Responder Authentication Address
            CustomLabel {
                visible: passwordDialogData.contains("ResponderAuthAddress")
                Layout.fillWidth: true
                text: qsTr("Responder\nAuthentication Address")
            }
            CustomLabelValue {
                visible: passwordDialogData.contains("ResponderAuthAddress")
                text: passwordDialogData.value("ResponderAuthAddress")
                Layout.alignment: Qt.AlignRight
            }

        }

        GridLayout {
            id: gridTxDetails
            columns: 2
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            rowSpacing: 0

            CustomHeader {
                visible: passwordDialogData.contains("TransactionAmount")
                           || passwordDialogData.contains("NetworkFee")
                           || passwordDialogData.contains("TotalSpent")

                Layout.fillWidth: true
                Layout.columnSpan: 2
                text: qsTr("Transaction Details")
                Layout.preferredHeight: 25
            }

            // TX Amount
            CustomLabel {
                visible: passwordDialogData.contains("TransactionAmount")
                Layout.fillWidth: true
                text: qsTr("Transaction Amount")
            }
            CustomLabelValue {
                visible: passwordDialogData.contains("TransactionAmount")
                text: passwordDialogData.value("TransactionAmount")
                Layout.alignment: Qt.AlignRight
            }

            // Network Fee
            CustomLabel {
                visible: passwordDialogData.contains("NetworkFee")
                Layout.fillWidth: true
                text: qsTr("Network Fee")
            }
            CustomLabelValue {
                visible: passwordDialogData.contains("NetworkFee")
                text: passwordDialogData.value("NetworkFee")
                Layout.alignment: Qt.AlignRight
            }

            // Total Spent
            CustomLabel {
                visible: passwordDialogData.contains("TotalSpent")
                Layout.fillWidth: true
                text: qsTr("Total Spent")
            }
            CustomLabelValue {
                visible: passwordDialogData.contains("TotalSpent")
                text: passwordDialogData.value("TotalSpent")
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
                property real timeLeft: 120
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
                text: qsTr("%1 seconds left").arg(timer.timeLeft.toFixed((0)))
                Layout.fillWidth: true
            }

            CustomProgressBar {
                Layout.minimumHeight: 6
                Layout.preferredHeight: 6
                Layout.maximumHeight: 6
                Layout.bottomMargin: 10
                Layout.fillWidth: true
                to: 120
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
                    if (authSign) {
                        authSign.cancel()
                    }
                }
            }

            CustomButtonPrimary {
                id: btnConfirm
                text: walletInfo.encType === QPasswordData.Password ? qsTr("CONFIRM") : qsTr("Continue")
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                enabled: tfPassword.text.length || acceptable
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
