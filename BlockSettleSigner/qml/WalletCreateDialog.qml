import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2
import com.blocksettle.PasswordConfirmValidator 1.0
import com.blocksettle.WalletSeed 1.0
import com.blocksettle.WalletInfo 1.0
import com.blocksettle.AuthProxy 1.0
import com.blocksettle.AuthSignWalletObject 1.0

import "bscontrols"

CustomDialog {
    property bool primaryWalletExists: false
    property string password
    property bool isPrimary:    false
    property WalletSeed seed
    property AuthSignWalletObject  authSign
    property bool acceptable:   tfName.text.length &&
                                newPasswordWithConfirm.acceptableInput
    property int inputLabelsWidth: 110

    id:root
    implicitWidth: 400
    implicitHeight: mainLayout.implicitHeight

    Component.onCompleted: {
        tfName.text = qsTr("Wallet #%1").arg(walletsProxy.walletNames.length + 1);
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
                root.close();
                event.accepted = true;
            }
        }

        ColumnLayout {
            anchors.fill: parent
            Layout.fillHeight: true
            Layout.fillWidth: true
            spacing: 10
            width: parent.width
            id: mainLayout

            RowLayout{
                CustomHeaderPanel{
                    id: panelHeader
                    Layout.preferredHeight: 40
                    Layout.fillWidth: true
                    text:  qsTr("Create New Wallet")
                }
            }
            CustomHeader {
                id: headerText
                text:   qsTr("Wallet Details")
                Layout.fillWidth: true
                Layout.preferredHeight: 25
                Layout.topMargin: 5
                Layout.leftMargin: 10
                Layout.rightMargin: 10
            }
            RowLayout {
                spacing: 5
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10

                CustomRadioButton {
                    id: rbMainNet
                    text:   qsTr("MainNet")
                    checked:    seed.mainNet
                    onClicked: {
                        seed.mainNet = true
                    }
                }
                CustomRadioButton {
                    id: rbTestNet
                    text:   qsTr("TestNet")
                    checked:    seed.testNet
                    onClicked: {
                        seed.testNet = true
                    }
                }
            }

            RowLayout {
                spacing: 5
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10

                CustomLabel {
                    Layout.minimumWidth: inputLabelsWidth
                    Layout.preferredWidth: inputLabelsWidth
                    Layout.maximumWidth: inputLabelsWidth
                    Layout.fillWidth: true
                    text:   qsTr("Wallet Name")
                }
                CustomTextInput {
                    id: tfName
                    Layout.fillWidth: true
                    selectByMouse: true
                    focus: true
                    onEditingFinished: {
                        seed.walletName = tfName.text
                    }
                }
            }
            RowLayout {
                spacing: 5
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10

                CustomLabel {
                    Layout.minimumWidth: inputLabelsWidth
                    Layout.preferredWidth: inputLabelsWidth
                    Layout.maximumWidth: inputLabelsWidth
                    Layout.fillWidth: true
                    text:   qsTr("Wallet Description")
                }
                CustomTextInput {
                    id: tfDesc
                    Layout.fillWidth: true
                    selectByMouse: true
                    validator: RegExpValidator {
                        regExp: /^[^\\\\/?:*<>|]*$/
                    }
                    onEditingFinished: {
                        seed.walletDesc = tfDesc.text
                    }
                }
            }
            RowLayout {
                spacing: 5
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10

                CustomCheckBox {
                    id: cbPrimary
                    Layout.fillWidth: true
                    Layout.leftMargin: inputLabelsWidth + 5
                    enabled: !primaryWalletExists
                    text:   qsTr("Primary Wallet")
                }
            }
            CustomHeader {
                id: headerText2
                text:   qsTr("Create Wallet Keys")
                Layout.fillWidth: true
                Layout.preferredHeight: 25
                Layout.topMargin: 5
                Layout.leftMargin: 10
                Layout.rightMargin: 10
            }
            RowLayout {
                spacing: 5
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10

                CustomRadioButton {
                    id: rbPassword
                    text:   qsTr("Password")
                    checked:    true
                }
                CustomRadioButton {
                    id: rbAuth
                    text:   qsTr("Auth eID")
                }
            }

            BSConfirmedPasswordInput {
                id: newPasswordWithConfirm
                visible:    rbPassword.checked
                columnSpacing: 10
                passwordLabelTxt: qsTr("Wallet Password")
                passwordInputPlaceholder: qsTr("New Wallet Password")
                confirmLabelTxt: qsTr("Confirm Password")
                confirmInputPlaceholder: qsTr("Confirm New Wallet Password")
            }

            RowLayout {
                visible:    rbAuth.checked
                spacing: 5
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10

                CustomTextInput {
                    id: tiAuthId
                    placeholderText: qsTr("Auth ID (email)")
                }
                CustomButton {
                    id: btnAuth
                    text:   !authSign ? qsTr("Sign with Auth eID") : authSign.status
                    enabled:    !authSign && tiAuthId.text.length
                    onClicked: {
                        seed.encType = WalletInfo.Auth
                        seed.encKey = tiAuthId.text
                        authSign = auth.signWallet(tiAuthId.text, qsTr("Password for wallet %1").arg(tfName.text),
                                                              seed.walletId)
                        btnAuth.enabled = false
                        authSign.success.connect(function(key) {
                            acceptable = true
                            password = key
                            text = qsTr("Successfully signed")
                        })
                        authSign.error.connect(function(text) {
                            authSign = null
                            btnAuth.enabled = tiAuthId.text.length
                        })
                    }
                }
            }

            Rectangle {
                Layout.fillHeight: true
            }

            CustomButtonBar {
                implicitHeight: childrenRect.height
                implicitWidth: root.width
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
                        text:   qsTr("Continue")
                        enabled:    acceptable
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
                            onClicked: root.reject();
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
        seed.walletName = tfName.text
        seed.walletDesc = tfDesc.text
        isPrimary = cbPrimary.checked
        if (rbPassword.checked) {
            seed.encType = WalletInfo.Password
            password = newPasswordWithConfirm.text
        }
    }

    onRejected: {
        authSign.cancel()
    }
}
