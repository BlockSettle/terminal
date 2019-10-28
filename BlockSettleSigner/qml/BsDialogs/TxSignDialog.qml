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
import "../BsStyles"
import "../js/helper.js" as JsHelper

CustomTitleDialogWindow {
    property WalletInfo walletInfo: WalletInfo {}
    property TXInfo txInfo: TXInfo {}
    property PasswordDialogData passwordDialogData: PasswordDialogData {}
    property QPasswordData passwordData: QPasswordData {}
    property AuthSignWalletObject authSign: AuthSignWalletObject {}

    property bool acceptable: walletInfo.encType === QPasswordData.Password ? tfPassword.text : true
    property int addressRowHeight: 24
    property int recipientsAddrHeight: txInfo.allRecipients.length < 4 ? txInfo.allRecipients.length * addressRowHeight : addressRowHeight * 3

    readonly property int duration: authSign.defaultExpiration()
    property real timeLeft: duration

    id: root
    title: qsTr("Sign Transaction")
    rejectable: true
    width: 500

    function init() {
        if (walletInfo.encType !== QPasswordData.Auth) {
            return
        }

        btnConfirm.visible = false
        btnCancel.anchors.horizontalCenter = barFooter.horizontalCenter

        authSign = qmlFactory.createAutheIDSignObject(AutheIDClient.SignWallet, walletInfo, timeLeft - authSign.networkDelayFix())

        authSign.succeeded.connect(function(encKey, password) {
            passwordData.encType = QPasswordData.Auth
            passwordData.encKey = encKey
            passwordData.binaryPassword = password
            acceptAnimated()
        });
        authSign.failed.connect(function(errorText) {
            var mb = JsHelper.messageBox(BSMessageBox.Type.Critical
                , qsTr("Wallet"), errorText
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

    onBsRejected: {
        if (authSign) {
            authSign.cancel()
        }
    }

    cContentItem: ColumnLayout {
        spacing: 10
        Layout.alignment: Qt.AlignTop

        GridLayout {
            id: gridDashboard
            columns: 2
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            rowSpacing: 0

            CustomHeader {
                Layout.fillWidth: true
                Layout.columnSpan: 2
                text: qsTr("Details")
                Layout.preferredHeight: 25
            }

            CustomLabel {
                Layout.fillWidth: true
                text: qsTr("Wallet")
            }
            CustomLabelValue {
                text: walletInfo.name
                Layout.alignment: Qt.AlignRight
            }

            CustomLabel {
                Layout.fillWidth: true
                text: qsTr("No. of Inputs")
            }
            CustomLabelValue {
                text: txInfo.nbInputs
                Layout.alignment: Qt.AlignRight
            }

            RowLayout {
                Layout.columnSpan: 2
                Layout.fillWidth: true

                CustomLabel {
                    text: qsTr("Output Address(es)")
                    Layout.alignment: Qt.AlignTop
                }

                ListView {
                    id: recipients
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignRight
                    model: txInfo.allRecipients
                    clip: true
                    Layout.preferredHeight: recipientsAddrHeight

                    flickableDirection: Flickable.VerticalFlick
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {
                        active: true
                    }

                    delegate: Rectangle {
                        id: addressRect
                        color: "transparent"
                        height: 22
                        width: recipients.width

                        CustomLabelValue {
                            id: labelTxWalletId
                            text: modelData
                            anchors.fill: addressRect
                            horizontalAlignment: Text.AlignRight
                            verticalAlignment: Text.AlignTop
                            font: fixedFont
                        }
                    }
                }
            }


            CustomLabel {
                Layout.fillWidth: true
                text: qsTr("Virtual Tx Size")
            }
            CustomLabelValue {
                text: txInfo.txVirtSize
                Layout.alignment: Qt.AlignRight
            }

            CustomLabel {
                Layout.fillWidth: true
                text: qsTr("Input Amount")
            }
            CustomLabelValue {
                text: txInfo.inputAmount.toFixed(8)
                Layout.alignment: Qt.AlignRight
            }

            CustomLabel {
                Layout.fillWidth: true
                text: qsTr("Transaction Amount")
            }
            CustomLabelValue {
                text: txInfo.amount.toFixed(8)
                Layout.alignment: Qt.AlignRight
            }

            CustomLabel {
                Layout.fillWidth: true
                text: qsTr("Return Amount")
            }
            CustomLabelValue {
                text: txInfo.changeAmount.toFixed(8)
                Layout.alignment: Qt.AlignRight
            }

            CustomLabel {
                Layout.fillWidth: true
                text: qsTr("Transaction Fee")
            }
            CustomLabelValue {
                text: txInfo.fee.toFixed(8)
                Layout.alignment: Qt.AlignRight
            }

            CustomLabel {
                Layout.fillWidth: true
                text: qsTr("Total Spent")
            }
            CustomLabelValue {
                text: txInfo.total.toFixed(8)
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
                //text: walletInfo.encType !== QPasswordData.Auth ? qsTr("Password Confirmation") : qsTr("Press Continue to start eID Auth")
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
        }

        RowLayout {
            spacing: 25
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomLabel {
                visible: walletInfo.encType === QPasswordData.Auth
                Layout.fillWidth: true
                text: qsTr("Auth eID")
            }

            CustomLabel {
                visible: walletInfo.encType === QPasswordData.Auth
                Layout.alignment: Qt.AlignRight
                text: walletInfo.email()
            }
        }

        ColumnLayout {
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            Timer {
                id: timer
                interval: 500
                running: true
                repeat: true
                onTriggered: {
                    timeLeft -= 0.5
                    if (timeLeft <= 0) {
                        stop()
                        rejectAnimated()
                    }
                }
                signal expired()
            }

            CustomProgressBar {
                Layout.minimumHeight: 6
                Layout.preferredHeight: 6
                Layout.maximumHeight: 6
                Layout.bottomMargin: 10
                Layout.fillWidth: true
                to: 120
                value: timeLeft
            }

            CustomLabelValue {
                text: qsTr("%1 seconds left").arg(timeLeft.toFixed((0)))
                Layout.fillWidth: true
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

            CustomButton {
                id: btnConfirm
                primary: true
                text: walletInfo.encType === QPasswordData.Password ? qsTr("CONFIRM") : qsTr("Continue")
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                enabled: tfPassword.text.length || acceptable
                onClicked: {
                    passwordData.textPassword = tfPassword.text
                    passwordData.encType = QPasswordData.Password
                    acceptAnimated()
                }
            }
        }
    }

}
