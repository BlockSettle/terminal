import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2
import com.blocksettle.PasswordConfirmValidator 1.0
import com.blocksettle.AuthProxy 1.0
import com.blocksettle.AuthSignWalletObject 1.0
import com.blocksettle.WalletInfo 1.0

import "bscontrols"


CustomDialog {
    id: changeWalletPasswordDialog

    property WalletInfo wallet
    property string oldPassword
    property string newPassword
    property bool acceptable: newPasswordWithConfirm.acceptableInput &&
                              tfOldPassword.text.length
    property int inputLablesWidth: 110
    property AuthSignWalletObject  authSignOld
    property AuthSignWalletObject  authSignNew

    implicitWidth: 400
    implicitHeight: mainLayout.implicitHeight

    onWalletChanged: {
        if (wallet.encType === WalletInfo.Auth) {
            authSignOld = auth.signWallet(wallet.encKey, qsTr("Old password for wallet %1").arg(wallet.name),
                                         wallet.rootId)

            authSignOld.success.connect(function(key) {
                oldPassword = key
                labelAuthStatus.text = qsTr("Old password ok")
            })
            authSignOld.error.connect(function(text) {
                changeWalletPasswordDialog.reject()
            })
        }
    }

    FocusScope {
        anchors.fill: parent
        focus: true

        Keys.onPressed: {
            if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                if (acceptable) {
                    accept();
                }

                event.accepted = true;
            } else if (event.key === Qt.Key_Escape) {
                changeWalletPasswordDialog.close();
                event.accepted = true;
            }
        }

        ColumnLayout {

            id: mainLayout
            Layout.fillWidth: true
            spacing: 10

            RowLayout{
                CustomHeaderPanel{
                    id: panelHeader
                    Layout.preferredHeight: 40
                    Layout.fillWidth: true
                    text:   qsTr("Change Password for Wallet %1").arg(wallet.name)

                }
            }

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
                        authSignNew = auth.signWallet(tiNewAuthId.text, qsTr("New password for wallet %1").arg(wallet.name),
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

            CustomButtonBar {
                implicitHeight: childrenRect.height
                implicitWidth: changeWalletPasswordDialog.width
                id: rowButtons

                Flow {
                    id: buttonRow
                    spacing: 5
                    padding: 5
                    height: childrenRect.height + 10
                    width: parent.width - buttonRowLeft - 5
                    LayoutMirroring.enabled: true
                    LayoutMirroring.childrenInherit: true
                    anchors.left: parent.left   // anchor left becomes right


                    CustomButtonPrimary {
                        Layout.fillWidth: true
                        text:   qsTr("CONFIRM")
                        enabled: acceptable
                        onClicked: {
                            accept()
                        }
                    }
                }

                Flow {
                    id: buttonRowLeft
                    spacing: 5
                    padding: 5
                    height: childrenRect.height + 10


                    CustomButton {
                        Layout.fillWidth: true
                        text:   qsTr("Cancel")
                        onClicked: {
                            onClicked: changeWalletPasswordDialog.reject();
                        }
                    }

                }
            }
        }
    }

    function toHex(str) {
        var hex = '';
        for(var i = 0; i < str.length; i++) {
            hex += ''+str.charCodeAt(i).toString(16);
        }
        return hex;
    }

    onAccepted: {
        if (wallet.encType === WalletInfo.Password) {
            oldPassword = toHex(tfOldPassword.text)
        }
        if (rbPassword.checked) {
            newPassword = toHex(newPasswordWithConfirm.text)
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
