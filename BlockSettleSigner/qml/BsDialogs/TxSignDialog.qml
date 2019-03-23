import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2

import com.blocksettle.TXInfo 1.0
import com.blocksettle.AutheIDClient 1.0
import com.blocksettle.AuthSignWalletObject 1.0
import com.blocksettle.WalletInfo 1.0
import com.blocksettle.QSeed 1.0
import com.blocksettle.QPasswordData 1.0
import com.blocksettle.NsWallet.namespace 1.0

import "../StyledControls"
import "../BsControls"
import "../js/helper.js" as JsHelper

CustomTitleDialogWindow {
    property string prompt
    property WalletInfo walletInfo: WalletInfo{}
    property TXInfo txInfo
    property QPasswordData passwordData: QPasswordData{}
    property bool   acceptable: walletInfo.encType === NsWallet.Password ? tfPassword.text : true
    property bool   cancelledByUser: false
    property AuthSignWalletObject  authSign

    title: qsTr("Sign Transaction")
    rejectable: true
    width: 500

    function clickConfirmBtn() {
        btnConfirm.clicked()
    }

    function init() {
        if (walletInfo.encType !== NsWallet.Auth) {
            return
        }

        btnConfirm.visible = false
        btnCancel.anchors.horizontalCenter = barFooter.horizontalCenter

        authSign = qmlFactory.createAutheIDSignObject(AutheIDClient.SignWallet, walletInfo)

        authSign.succeeded.connect(function(encKey, password) {
            passwordData.encType = NsWallet.Auth
            passwordData.encKey = encKey
            passwordData.binaryPassword = password
            acceptAnimated()
        });
        authSign.failed.connect(function(errorText) {
            var mb = JsHelper.messageBox(BSMessageBox.Type.Critical
                , qsTr("Wallet"), qsTr("eID request failed with error: \n") + errorText
                , qsTr("Wallet Name: %1\nWallet ID: %2").arg(walletInfo.name).arg(walletInfo.rootId))
            mb.accepted.connect(function() { rejectAnimated() })
        })
        authSign.userCancelled.connect(function() {
            cancelledByUser = true
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

//    onWalletInfoChanged: {
//        if (walletInfo.encType === NsWallet.Auth) btnConfirm.clicked()
//    }


    cContentItem: ColumnLayout {
        spacing: 10

        CustomLabel {
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            visible: !txInfo.nbInputs && walletInfo.name.length
            text: qsTr("Wallet %1").arg(walletInfo.name)
        }

        GridLayout {
            id: gridDashboard
            visible: txInfo.nbInputs
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
                text: qsTr("Sending Wallet")
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

                CustomLabel {
                    Layout.fillWidth: true
                    text: qsTr("Receiving Address(es)")
                    //verticalAlignment: Text.AlignTop
                    Layout.alignment: Qt.AlignTop
                    Layout.fillHeight: true
                }
                ColumnLayout{
                    spacing: 0
                    Layout.leftMargin: 0
                    Layout.rightMargin: 0
                    Repeater {
                        Layout.alignment: Qt.AlignTop
                        model: txInfo.recvAddresses

                        Rectangle {
                            color: "transparent"
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            implicitWidth: labelTxWalletId.width
                            Layout.alignment: Qt.AlignLeft

                            CustomLabelValue {
                                id: labelTxWalletId
                                text: modelData
                                Layout.alignment: Qt.AlignLeft
                            }
                        }
                    }
                }

            }


            CustomLabel {
                Layout.fillWidth: true
                text: qsTr("Transaction Weight")
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
                text: qsTr("Transaction Amount")
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
                //text: walletInfo.encType !== NsWallet.Auth ? qsTr("Password Confirmation") : qsTr("Press Continue to start eID Auth")
                text: qsTr("Decrypt Wallet")
                Layout.preferredHeight: 25
            }
        }

        RowLayout {
            spacing: 25
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

//            CustomLabel {
//                visible: prompt.length
//                Layout.minimumWidth: 110
//                Layout.preferredWidth: 110
//                Layout.maximumWidth: 110
//                Layout.fillWidth: true
//                text: prompt
//                elide: Label.ElideRight
//            }

            CustomLabel {
                visible: walletInfo.encType === NsWallet.Password
                Layout.minimumWidth: 110
                Layout.preferredWidth: 110
                Layout.maximumWidth: 110
                Layout.fillWidth: true
                text: qsTr("Password")
            }

            CustomPasswordTextInput {
                id: tfPassword
                visible: walletInfo.encType === NsWallet.Password
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
                visible: walletInfo.encType === NsWallet.Auth
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
                        cancelledByUser = true
                        rejectAnimated()
                    }
                }
                signal expired()
            }

//            CustomLabel {
//                text: qsTr("On completion just press [Enter] or [Return]")
//                visible: walletInfo.encType !== NsWallet.Auth
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
                    cancelledByUser = true
                    rejectAnimated()
                    if (authSign) {
                        authSign.cancel()
                    }
                }
            }

            CustomButtonPrimary {
                id: btnConfirm
                text: walletInfo.encType === NsWallet.Password ? qsTr("CONFIRM") : qsTr("Continue")
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                enabled: tfPassword.text.length || acceptable
                onClicked: {
                    if (walletInfo.encType === NsWallet.Password) {
                        passwordData.textPassword = tfPassword.text
                        passwordData.encType = NsWallet.Password
                        acceptAnimated()
                    }
                    else if (walletInfo.encType === NsWallet.Auth) {
                    }
                    else {
                        passwordData.encType = NsWallet.Unencrypted
                        acceptAnimated()
                    }
                }
            }
        }
    }
}
