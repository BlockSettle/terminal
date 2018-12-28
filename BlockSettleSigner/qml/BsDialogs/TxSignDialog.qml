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

    title: qsTr("Wallet Password Confirmation")

    function clickConfirmBtn(){
        btnConfirm.clicked()
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
            visible: !txInfo.nbInputs && txInfo.walletInfo.name.length
            text: qsTr("Wallet %1").arg(txInfo.walletInfo.name)
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
                text: txInfo.walletInfo.name
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

            CustomLabel {
                Layout.fillWidth: true
                text: qsTr("Receiving Address(es)")
                verticalAlignment: Text.AlignTop
                Layout.fillHeight: true
            }
            ColumnLayout{
                spacing: 0
                Layout.leftMargin: 0
                Layout.rightMargin: 0
                Repeater {
                    model: txInfo.recvAddresses
                    CustomLabelValue {
                        text: modelData
                        Layout.alignment: Qt.AlignRight
                    }
                }
            }

            CustomLabel {
                Layout.fillWidth: true
                text: qsTr("Transaction Size")
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
                text: txInfo.walletInfo.encType !== NsWallet.Auth ? qsTr("Password Confirmation") : qsTr("Press Continue to start eID Auth")
                Layout.preferredHeight: 25
            }
        }

        RowLayout {
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomLabel {
                visible: prompt.length
                Layout.minimumWidth: 110
                Layout.preferredWidth: 110
                Layout.maximumWidth: 110
                Layout.fillWidth: true
                text: prompt
                elide: Label.ElideRight
            }

            CustomPasswordTextInput {
                id: tfPassword
                visible: txInfo.walletInfo.encType === NsWallet.Password
                focus: true
                placeholderText: qsTr("Password")
                echoMode: TextField.Password
                Layout.fillWidth: true
            }

            CustomLabel {
                id: labelAuth
                visible: txInfo.walletInfo.encType === NsWallet.Auth
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

            CustomLabel {
                text: qsTr("On completion just press [Enter] or [Return]")
                visible: txInfo.walletInfo.encType !== NsWallet.Auth
                Layout.fillWidth: true
            }
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
            Layout.fillWidth: true

            CustomButton {
                text: qsTr("Cancel")
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                onClicked: {
                    cancelledByUser = true
                    rejectAnimated()
                }
            }

            CustomButtonPrimary {
                id: btnConfirm
                text: txInfo.walletInfo.encType === NsWallet.Password ? qsTr("CONFIRM") : qsTr("Continue")
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                enabled: tfPassword.text.length || acceptable
                onClicked: {
                    if (txInfo.walletInfo.encType === NsWallet.Password) {
                        passwordData.textPassword = tfPassword.text
                        passwordData.encType = NsWallet.Password
                        acceptAnimated()
                    }
                    else if (txInfo.walletInfo.encType === NsWallet.Auth) {
                        JsHelper.requesteIdAuth(AutheIDClient.SignWallet
                                                , walletInfo
                                                , function(pd){
                                                    passwordData = pd
                                                    acceptAnimated()
                                                })

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
