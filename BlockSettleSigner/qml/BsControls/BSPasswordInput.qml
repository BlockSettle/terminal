import QtQuick 2.9
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.11

import "../StyledControls"
import "../BsStyles"

import com.blocksettle.PasswordDialogData 1.0
import com.blocksettle.AutheIDClient 1.0
import com.blocksettle.AuthSignWalletObject 1.0
import com.blocksettle.WalletInfo 1.0
import com.blocksettle.QPasswordData 1.0

CustomTitleDialogWindow {
    id: root

    property AuthSignWalletObject authSign: AuthSignWalletObject{}
    property QPasswordData passwordData: QPasswordData{}
    property int autheIDSignType: AutheIDClient.SignWallet

    property alias enteredPassword : passwordInput.text
    default property alias details: detailsContainer.data

    property alias btnReject : btnReject
    property alias btnAccept : btnAccept
    property alias passwordInput : passwordInput
    property string decryptHeaderText: qsTr("Decrypt Wallet")

    readonly property int duration: authSign.defaultExpiration()
    property real timeLeft: duration

    title: qsTr("Decrypt Wallet")
    width: 350
    rejectable: true

    function init() {
        if (walletInfo.encType !== QPasswordData.Auth) {
            return
        }

        btnAccept.visible = false
        btnReject.anchors.horizontalCenter = barFooter.horizontalCenter

        authSign = qmlFactory.createAutheIDSignObject(autheIDSignType, walletInfo, timeLeft - authSign.networkDelayFix())

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

    onBsRejected: {
        if (authSign) {
            authSign.cancel()
        }
    }

    cContentItem: ColumnLayout {
        id: contentItemData
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignTop
        Layout.margins: 0

        ColumnLayout {
            Layout.preferredWidth: root.width
            spacing: 0
            Layout.margins: 0
            Layout.alignment: Qt.AlignTop


            ColumnLayout {
                id: detailsContainer
                Layout.margins: 0
                spacing: 0
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
            }

            CustomHeader {
                id: decryptHeader
                Layout.alignment: Qt.AlignTop
                text: decryptHeaderText
                Layout.fillWidth: true
                Layout.preferredHeight: 25
                Layout.topMargin: 5
                Layout.leftMargin: 10
                Layout.rightMargin: 10
            }

            RowLayout {
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                Layout.topMargin: 5
                Layout.bottomMargin: 5

                CustomLabel {
                    visible: walletInfo.encType === QPasswordData.Password
                    Layout.fillWidth: true
                    Layout.minimumWidth: 110
                    Layout.preferredWidth: 110
                    Layout.maximumWidth: 110
                    text: qsTr("Password")
                }

                CustomTextInput {
                    id: passwordInput
                    visible: walletInfo.encType === QPasswordData.Password
                    Layout.fillWidth: true
                    Layout.topMargin: 5
                    Layout.bottomMargin: 5
                    focus: true
                    echoMode: TextField.Password
                    //placeholderText: qsTr("Password")


                    Keys.onEnterPressed: {
                        if (btnAccept.enabled) btnAccept.onClicked()
                    }
                    Keys.onReturnPressed: {
                        if (btnAccept.enabled) btnAccept.onClicked()
                    }
                }
            }

            ColumnLayout {
                visible: walletInfo.encType === QPasswordData.Auth
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
                    visible: walletInfo.encType === QPasswordData.Auth
                    Layout.minimumHeight: 6
                    Layout.preferredHeight: 6
                    Layout.maximumHeight: 6
                    Layout.bottomMargin: 10
                    Layout.fillWidth: true
                    to: duration
                    value: timeLeft
                }

                CustomLabelValue {
                    visible: walletInfo.encType === QPasswordData.Auth
                    text: qsTr("%1 seconds left").arg(timeLeft.toFixed(0))
                    Layout.fillWidth: true
                }
            }

        }
    }

    cFooterItem: RowLayout {
        Layout.fillWidth: true
        CustomButtonBar {
            id: barFooter
            Layout.fillWidth: true

            CustomButton {
                id: btnReject
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.margins: 5
                text: qsTr("Cancel")
                onClicked: {
                    rejectAnimated()
                }
            }

            CustomButtonPrimary {
                id: btnAccept
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 5
                text: qsTr("Ok")
                onClicked: {
                    if (walletInfo.encType === QPasswordData.Password) {
                        passwordData.textPassword = passwordInput.text
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

