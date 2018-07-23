import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2
import com.blocksettle.PasswordConfirmValidator 1.0
import com.blocksettle.FrejaProxy 1.0
import com.blocksettle.FrejaSignWalletObject 1.0
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
    property FrejaSignWalletObject  frejaSignOld
    property FrejaSignWalletObject  frejaSignNew

    implicitWidth: 400
    implicitHeight: mainLayout.implicitHeight

    onWalletChanged: {
        if (wallet.encType === WalletInfo.Freja) {
            frejaSignOld = freja.signWallet(wallet.encKey, qsTr("Old password for wallet %1").arg(wallet.name),
                                         wallet.rootId)

            frejaSignOld.success.connect(function(key) {
                oldPassword = key
                labelFrejaStatus.text = qsTr("Old password ok")
            })
            frejaSignOld.error.connect(function(text) {
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
                    text: qsTr("Current password:")
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
                    id: labelFreja
                    visible: wallet.encType === WalletInfo.Freja
                    text: qsTr("Sign with Freja eID")
                }
                CustomLabel {
                    id: labelFrejaStatus
                    visible: wallet.encType === WalletInfo.Freja
                    text: frejaSignOld.status
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
                    id: rbFreja
                    text: qsTr("Freja eID")
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
                visible:    rbFreja.checked

                CustomTextInput {
                    id: tiNewFrejaId
                    placeholderText: qsTr("New Freja ID (email)")
                }
                CustomButton {
                    id: btnFrejaNew
                    text:   !frejaSignNew ? qsTr("Sign with Freja") : frejaSignNew.status
                    enabled:    !frejaSignNew && tiNewFrejaId.text.length
                    onClicked: {
                        frejaSignNew = freja.signWallet(tiNewFrejaId.text, qsTr("New password for wallet %1").arg(wallet.name),
                                                              wallet.rootId)
                        btnFrejaNew.enabled = false
                        frejaSignNew.success.connect(function(key) {
                            acceptable = true
                            newPassword = key
                            text = qsTr("Successfully signed")
                        })
                        frejaSignNew.error.connect(function(text) {
                            frejaSignNew = null
                            btnFrejaNew.enabled = tiNewFrejaId.text.length
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
        else if (rbFreja.checked) {
            wallet.encType = WalletInfo.Freja
            wallet.encKey = tiNewFrejaId.text
        }
    }

    onRejected: {
        frejaSignOld.cancel()
        frejaSignNew.cancel()
    }
}
