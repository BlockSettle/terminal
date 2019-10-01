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
    property QPasswordData passwordData: QPasswordData {}
    property AuthSignWalletObject  authSign: AuthSignWalletObject {}

    property bool signingAllowed: passwordDialogData.SigningAllowed

    // rfq details
    readonly property string product: passwordDialogData.Product
    readonly property string productGroup: passwordDialogData.ProductGroup
    readonly property string security: passwordDialogData.Security
    readonly property string side: passwordDialogData.Side
    property string quantity
    property string price: passwordDialogData.Price
    property string priceString
    property string totalValue

    property alias settlementDetailsItem: settlementDetailsContainer.data
    property alias txDetailsItem: txDetailsContainer.data

    readonly property bool acceptable: walletInfo.encType === QPasswordData.Password ? tfPassword.text : true
    readonly property int addressRowHeight: 24

    readonly property int duration: passwordDialogData.Duration / 1000.0 - 1 > 0 ? passwordDialogData.Duration / 1000.0 - 1 : 60
    readonly property real balanceDivider : qmlFactory.balanceDivider()

    readonly property bool is_sell: side === "SELL"
    readonly property bool is_buy: side === "BUY"

    readonly property string minus_string: ""  // "- "
    readonly property string plus_string: ""   // "+ "

    id: root
    title: passwordDialogData.Title
    rejectable: true
    width: 500

    function init() {
        if (walletInfo.encType === QPasswordData.Password) {
            btnConfirm.visible = false
            btnCancel.anchors.horizontalCenter = barFooter.horizontalCenter
        }
    }

    function initAuth() {
        if (walletInfo.encType !== QPasswordData.Auth) {
            return
        }

        authSign = qmlFactory.createAutheIDSignObject(AutheIDClient.SettlementTransaction, walletInfo)

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
            console.log("TxSignSettlementBaseDialog.qml, cancel requested for id=" + settlementId)
            if (settlementId === passwordDialogData.SettlementId) {
                rejectAnimated()
            }
        }
    }

    onBsRejected: {
        if (authSign) {
            authSign.cancel()
        }
    }

    onSigningAllowedChanged: {
        if (signingAllowed) {
            initAuth()
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
                text: productGroup
                Layout.alignment: Qt.AlignRight
            }

            // Security ID
            CustomLabel {
                Layout.fillWidth: true
                text: qsTr("Security ID")
            }
            CustomLabelValue {
                text: security
                Layout.alignment: Qt.AlignRight
            }

            // Product
            CustomLabel {
                Layout.fillWidth: true
                text: qsTr("Product")
            }
            CustomLabelValue {
                text: product
                Layout.alignment: Qt.AlignRight
            }

            // Side
            CustomLabel {
                Layout.fillWidth: true
                text: qsTr("Side")
            }
            CustomLabelValue {
                text: side
                Layout.alignment: Qt.AlignRight
            }

            // Quantity
            CustomLabel {
                Layout.fillWidth: true
                text: qsTr("Quantity")
            }
            CustomLabelValue {
                text: quantity
                Layout.alignment: Qt.AlignRight
            }

            // Price
            CustomLabel {
                Layout.fillWidth: true
                text: qsTr("Price")
            }
            CustomLabelValue {
                text: priceString
                Layout.alignment: Qt.AlignRight
            }

            // Total Value
            CustomLabel {
                Layout.fillWidth: true
                text: qsTr("Total Value")
            }
            CustomLabelValue {
                text: totalValue
                Layout.alignment: Qt.AlignRight
            }
        }

        ColumnLayout {
            id: settlementDetailsContainer
            Layout.alignment: Qt.AlignTop
            Layout.margins: 0
            spacing: 0
            clip: true
        }

        ColumnLayout {
            id: txDetailsContainer
            Layout.alignment: Qt.AlignTop
            Layout.margins: 0
            spacing: 0
            clip: true
        }

        ColumnLayout {
            spacing: 0
            Layout.fillWidth: true
            Layout.topMargin: 5
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomHeader {
                Layout.fillWidth: true
                text: qsTr("Decrypt Wallet")
                Layout.preferredHeight: 25
            }

            RowLayout {
                spacing: 5
                Layout.fillWidth: true

                CustomLabel {
                    Layout.fillWidth: true
                    text: qsTr("Wallet name")
                }
                CustomLabel {
                    //Layout.fillWidth: true
                    Layout.alignment: Qt.AlignRight
                    text: walletInfo.name
                }
            }

            RowLayout {
                spacing: 5
                Layout.fillWidth: true

                CustomLabel {
                    Layout.fillWidth: true
                    text: qsTr("Wallet ID")
                }
                CustomLabel {
                    //Layout.fillWidth: true
                    Layout.alignment: Qt.AlignRight
                    text: walletInfo.walletId
                }
            }

            RowLayout {
                spacing: 5
                Layout.fillWidth: true
                visible: walletInfo.encType === QPasswordData.Auth

                CustomLabel {
                    Layout.fillWidth: true
                    text: qsTr("Auth eID Email")
                    visible: walletInfo.encType === QPasswordData.Auth
                }
                CustomLabel {
                    Layout.alignment: Qt.AlignRight
                    // text: walletInfo.email()
                    visible: walletInfo.encType === QPasswordData.Auth
                }
            }

            RowLayout {
                spacing: 25
                Layout.fillWidth: true

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
            }

            ColumnLayout {
                spacing: 5
                Layout.fillWidth: true

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

                CustomProgressBar {
                    Layout.minimumHeight: 6
                    Layout.preferredHeight: 6
                    Layout.maximumHeight: 6
                    Layout.topMargin: 10
                    Layout.fillWidth: true
                    to: duration
                    value: timer.timeLeft
                }

                CustomLabelValue {
                    text: signingAllowed ? qsTr("%1 seconds left").arg(timer.timeLeft.toFixed(0)) : qsTr("Authentication Address could not be verified")
                    Layout.fillWidth: true
                }
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
                enabled: (signingAllowed && (tfPassword.text.length || acceptable))
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
