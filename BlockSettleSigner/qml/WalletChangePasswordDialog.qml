import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2
import com.blocksettle.PasswordConfirmValidator 1.0
import com.blocksettle.AuthProxy 1.0
import com.blocksettle.AuthSignWalletObject 1.0
import com.blocksettle.WalletInfo 1.0
import com.blocksettle.WalletSeed 1.0
import com.blocksettle.AutheIDClient 1.0

import "BsControls"
import "StyledControls"
import "js/helper.js" as JsHelper

CustomTitleDialogWindow {
    id: changeWalletPasswordDialog

    property WalletInfo wallet
    property string oldPassword
    property string newPassword
    property bool acceptable: newPasswordWithConfirm.acceptableInput &&
                              tfOldPassword.text.length
    property int inputLablesWidth: 110
    property AuthSignWalletObject  authSignOld
    property AuthSignWalletObject  authSignNew

    title:   qsTr("Change Password for Wallet <%1>").arg(wallet.name)
    width: 400

    onWalletChanged: {
        if (wallet.encType === WalletInfo.Auth) {
            authSignOld = authProxy.signWallet(wallet.encKey, qsTr("Old password for wallet %1").arg(wallet.name),
                                         wallet.rootId)

            authSignOld.success.connect(function(key) {
                oldPassword = key
                labelAuthStatus.text = qsTr("Old password ok")
            })
            authSignOld.error.connect(function(text) {
                changeWalletPasswordDialog.rejectAnimated()
            })
        }
    }

    cContentItem: ColumnLayout {
        id: mainLayout
        spacing: 10

        RowLayout {
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomLabel {
                visible:    wallet.encType === WalletInfo.Password
                elide: Label.ElideRight
                text: qsTr("Current password")
                wrapMode: Text.WordWrap
                Layout.minimumWidth: 110
                Layout.preferredWidth: 110
                Layout.maximumWidth: 110
                Layout.fillWidth: true
            }
            CustomTextInput {
                id: tfOldPassword
                visible: wallet.encType === WalletInfo.Password
                focus: true
                placeholderText: qsTr("Old Password")
                echoMode: TextField.Password
                Layout.fillWidth: true
            }

            CustomLabel {
                id: labelAuth
                visible: wallet.encType === WalletInfo.Auth
                text: qsTr("Sign with Auth eID")
            }
            CustomLabel {
                id: labelAuthStatus
                visible: wallet.encType === WalletInfo.Auth
                text: authSignOld.status
            }
        }

        RowLayout {
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomRadioButton {
                id: rbPassword
                text: qsTr("Password")
                checked: true
            }
            CustomRadioButton {
                id: rbAuth
                text: qsTr("Auth eID")
                checked: false
            }
        }

        BSConfirmedPasswordInput {
            id: newPasswordWithConfirm
            columnSpacing: 10
            visible: rbPassword.checked
            passwordLabelTxt: qsTr("New Password")
            passwordInputPlaceholder: qsTr("New Password")
            confirmLabelTxt: qsTr("Confirm New")
            confirmInputPlaceholder: qsTr("Confirm New Password")
        }

        RowLayout {
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            visible:    rbAuth.checked

            CustomTextInput {
                id: tiNewAuthId
                placeholderText: qsTr("New Auth ID (email)")
            }
            CustomButton {
                id: btnAuthNew
                text:   !authSignNew ? qsTr("Sign with Auth eID") : authSignNew.status
                enabled:    !authSignNew && tiNewAuthId.text.length
                onClicked: {
                    authSignNew = authProxy.signWallet(AutheIDClient.ActivateWallet, tiNewAuthId.text, qsTr("New password for wallet %1").arg(wallet.name),
                                                          wallet.rootId)
                    btnAuthNew.enabled = false
                    authSignNew.success.connect(function(key) {
                        acceptable = true
                        newPassword = key
                        text = qsTr("Successfully signed")
                    })
                    authSignNew.error.connect(function(text) {
                        authSignNew = null
                        btnAuthNew.enabled = tiNewAuthId.text.length
                    })
                }
            }
        }
    }

    cFooterItem: RowLayout {
        CustomButtonBar {
            Layout.fillWidth: true

            id: rowButtons

            CustomButton {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                text:   qsTr("Cancel")
                onClicked: {
                    rejectAnimated()
                }
            }

            CustomButtonPrimary {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                text:   qsTr("CONFIRM")
                enabled: acceptable
                onClicked: {
                    acceptAnimated()
                }
            }
        }

    }

    onAccepted: {
        if (wallet.encType === WalletInfo.Password) {
            oldPassword = JsHelper.toHex(tfOldPassword.text)
        }
        if (rbPassword.checked) {
            newPassword = JsHelper.toHex(newPasswordWithConfirm.text)
        }
        else if (rbAuth.checked) {
            wallet.encType = WalletInfo.Auth
            wallet.encKey = tiNewAuthId.text
        }
    }

    onRejected: {
        authSignOld.cancel()
        authSignNew.cancel()
    }
}
