/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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
import com.blocksettle.HwDeviceManager 1.0

import "../StyledControls"
import "../BsControls"
import "../BsStyles"
import "../BsHw"
import "../js/helper.js" as JsHelper

BSWalletHandlerDialog {
    property WalletInfo walletInfo: WalletInfo {}
    property TXInfo txInfo: TXInfo {}
    property PasswordDialogData passwordDialogData: PasswordDialogData {}
    property QPasswordData passwordData: QPasswordData {}
    property AuthSignWalletObject authSign: AuthSignWalletObject {}

    property bool acceptable: if (walletInfo.encType === QPasswordData.Password)
                                  tfPassword.text.length
                              else if (walletInfo.encType === QPasswordData.Hardware)
                                  passwordData.encType === QPasswordData.Hardware
                              else
                                  true

    property int addressRowHeight: 24
    property int recipientsAddrHeight: txInfo.allRecipients.length < 4 ? txInfo.allRecipients.length * addressRowHeight : addressRowHeight * 3

    readonly property int duration: authSign.defaultExpiration()
    property real timeLeft: duration

    property string hwDeviceStatus: qsTr("Searching for device")

    id: root
    title: qsTr("Sign Transaction")
    rejectable: true
    width: 500

    onAboutToHide: hwDeviceManager.hwOperationDone()

    function init() {
        if (walletInfo.encType === QPasswordData.Auth) {
            btnConfirm.visible = false
            btnCancel.anchors.horizontalCenter = barFooter.horizontalCenter

            let authEidMessage = JsHelper.getAuthEidTransactionInfo(txInfo);
            authSign = qmlFactory.createAutheIDSignObject(AutheIDClient.SignWallet, walletInfo,
                                                          authEidMessage, timeLeft)

            authSign.succeeded.connect(function(encKey, password) {
                passwordData.encType = QPasswordData.Auth
                passwordData.encKey = encKey
                passwordData.binaryPassword = password
                acceptAnimated()
            });
            authSign.failed.connect(function(errorText) {
                showWalletError(errorText);
            })
            authSign.userCancelled.connect(function() {
                rejectWithNoError();
            })
            authSign.canceledByTimeout.connect(function() {
                rejectWithNoError();
            })
        }
        else if (walletInfo.encType === QPasswordData.Hardware) {
            hwDeviceManager.prepareHwDeviceForSign(walletInfo.walletId)
        }
    }

    Connections {
        target: hwDeviceManager
        onRequestPinMatrix: JsHelper.showHwPinMatrix(deviceIndex);
        onDeviceReady: hwDeviceManager.signTX(passwordDialogData.TxRequest);
        onRequestHWPass: JsHelper.showHwPassphrase(deviceIndex, allowedOnDevice);
        onDeviceNotFound: {
            hwDeviceStatus = "Searching for device"
            let lastDeviceError = hwDeviceManager.lastDeviceError(0);
            if (lastDeviceError.length > 0) {
                hwDeviceStatus += '('+ lastDeviceError + ')'
            }
            delayScanDevice.start();
        }
        onDeviceTxStatusChanged: hwDeviceStatus = status;
        onTxSigned: {
            passwordData.binaryPassword = signData
            passwordData.encType = QPasswordData.Hardware
            acceptAnimated();
        }
        onCancelledOnDevice: rejectAnimated()
        onInvalidPin: {
            var dlg = JsHelper.messageBox(BSMessageBox.Type.Critical, "Invalid PIN", "Please try again")
            dlg.bsAccepted.connect(function(){
                hwDeviceManager.signTX(passwordDialogData.TxRequest)
            })
        }
        onOperationFailed: showWalletError(reason)
    }

    Timer {
        id: delayScanDevice
        interval: 2000
        repeat: false
        onTriggered: {
            hwDeviceManager.prepareHwDeviceForSign(walletInfo.walletId)
        }
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
            visible: walletInfo.encType === QPasswordData.Auth

            CustomLabel {
                Layout.fillWidth: true
                text: qsTr("Auth eID")
            }

            CustomLabel {
                Layout.alignment: Qt.AlignRight
                text: walletInfo.email()
            }
        }

        RowLayout {
            spacing: 25
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            visible: walletInfo.encType === QPasswordData.Hardware

            CustomLabel {
                text: qsTr("Hardware Security Module")
            }

            CustomLabel {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignRight
                text: hwDeviceStatus
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
                        rejectWithNoError();
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
                text: qsTr("%1 seconds left").arg(Math.max(0, timeLeft.toFixed(0)))
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
                anchors.left: walletInfo.encType !== QPasswordData.Hardware ? parent.left : undefined
                anchors.right: walletInfo.encType === QPasswordData.Hardware ? parent.right : undefined
                anchors.bottom: parent.bottom
                onClicked: {
                    hwDeviceManager.releaseDevices()
                    if (walletInfo.encType === QPasswordData.Hardware &&
                            hwDeviceManager.awaitingUserAction(0)) {
                        let warning = JsHelper.showDropHwDeviceMessage();
                        warning.bsAccepted.connect(function(){ rejectAnimated() })
                    }
                    else {
                        rejectAnimated();
                    }
                }
            }

            CustomButton {
                id: btnConfirm
                primary: true
                visible: walletInfo.encType !== QPasswordData.Hardware
                text: walletInfo.encType !== QPasswordData.Auth ? qsTr("CONFIRM") : qsTr("Continue")
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                enabled: acceptable
                onClicked: {
                    passwordData.textPassword = tfPassword.text
                    passwordData.encType = QPasswordData.Password
                    acceptAnimated();
                }
            }
        }
    }

}
