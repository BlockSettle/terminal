import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.4
import com.blocksettle.PasswordConfirmValidator 1.0
import com.blocksettle.WalletSeed 1.0
import com.blocksettle.WalletInfo 1.0
import com.blocksettle.AuthProxy 1.0
import com.blocksettle.AuthSignWalletObject 1.0
import com.blocksettle.MobileClient 1.0

import "BsControls"
import "StyledControls"
import "js/helper.js" as JsHelper

CustomTitleDialogWindow {
    id: root

    property bool primaryWalletExists: false
    property string password
    property bool isPrimary:    false
    property WalletSeed seed
    property int encType
    property string encKey
    property int inputLabelsWidth: 110

    width: 400
    abortConfirmation: true
    abortBoxType: BSAbortBox.AbortType.WalletCreation
    title: qsTr("Create New Wallet")

    Component.onCompleted: {
        tfName.text = qsTr("Wallet #%1").arg(walletsProxy.walletNames.length + 1);
    }

    cContentItem: ColumnLayout {
        spacing: 10

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
                Layout.leftMargin: inputLabelsWidth
                checked: seed ? seed.mainNet : false
                onClicked: {
                    seed.mainNet = true
                }
            }
            CustomRadioButton {
                id: rbTestNet
                text:   qsTr("TestNet")
                checked: seed ? seed.testNet : false
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
                text:   qsTr("Wallet ID")
            }
            CustomLabel {
                Layout.fillWidth: true
                text: seed ? seed.walletId : qsTr("")
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
                text:   qsTr("Name")
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
                text:   qsTr("Description")
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
                //ToolTip.text: qsTr("A primary Wallet already exists, wallet will be created as regular wallet.")
                //ToolTip.delay: 1000
                //ToolTip.timeout: 5000
                //ToolTip.visible: hovered
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
                text: qsTr("Password")
                checked: true
                Layout.leftMargin: inputLabelsWidth
            }
            CustomRadioButton {
                id: rbAuth
                text: qsTr("Auth eID")
                onCheckedChanged: {
                    if (checked) {
                        // show notice dialog
                        var noticeEidDialog = Qt.createComponent("BsControls/BSEidNoticeBox.qml").createObject(mainWindow);
                        noticeEidDialog.open()
                    }
                }
            }
        }

        BSConfirmedPasswordInput {
            id: newPasswordWithConfirm
            visible:    rbPassword.checked
            columnSpacing: 10
            passwordLabelTxt: qsTr("Password")
            passwordInputPlaceholder: qsTr("Wallet Password")
            confirmLabelTxt: qsTr("Confirm Password")
            confirmInputPlaceholder: qsTr("Confirm Wallet Password")
        }

        RowLayout {
            visible: rbAuth.checked
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomLabel {
                Layout.minimumWidth: inputLabelsWidth
                Layout.preferredWidth: inputLabelsWidth
                Layout.maximumWidth: inputLabelsWidth
                Layout.fillWidth: true
                text:   qsTr("Auth eID email")
            }
            CustomTextInput {
                id: textInputEmail
                Layout.fillWidth: true
                selectByMouse: true
                focus: true
                onEditingFinished: {
                    seed.walletName = tfName.text
                }
            }
        }

        Rectangle {
            Layout.fillHeight: true
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
                    JsHelper.openAbortBox(root, abortBoxType)
                }
            }
            CustomButtonPrimary {
                text:   qsTr("Continue")
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                enabled: tfName.text.length &&
                            (newPasswordWithConfirm.acceptableInput && rbPassword.checked ||
                             textInputEmail.text && rbAuth.checked)

                onClicked: {
                    if (rbPassword.checked) {
                        // password
                        var checkPasswordDialog = Qt.createComponent("BsControls/BSPasswordInput.qml").createObject(mainWindow);
                        checkPasswordDialog.passwordToCheck = newPasswordWithConfirm.text
                        checkPasswordDialog.open()
                        checkPasswordDialog.accepted.connect(function(){
                            acceptAnimated()
                        })
                    }
                    else {
                        // auth eID
                        encType = WalletInfo.Auth

                        var authActivate = authProxy.signWallet(MobileClient.ActivateWallet,  textInputEmail.text, qsTr("Password for wallet %1").arg(tfName.text),
                                                                seed.walletId, "")
                        var activateProgress = Qt.createComponent("BsControls/BSEidProgressBox.qml").createObject(mainWindow);
                        activateProgress.title = qsTr("Activate Auth eID signing")
                        activateProgress.customDetails = qsTr("Wallet ID: %1").arg(seed ? seed.walletId : qsTr(""))
                        activateProgress.customText = qsTr("%1").arg(textInputEmail.text)
                        activateProgress.open()
                        activateProgress.rejected.connect(function() {
                            authActivate.destroy()
                        })

                        authActivate.succeeded.connect(function(encKey_, password_) {
                            console.log("authActivate.succeeded.connect " + encKey_)
                            console.log("authActivate.succeeded.connect " + password_)

                            activateProgress.acceptAnimated()
                            authActivate.destroy()

                            encKey = encKey_
                            seed.password = password_
                            //acceptable = true

                            var confirmEidDialog = Qt.createComponent("BsControls/BSEidConfirmBox.qml").createObject(mainWindow);
                            confirmEidDialog.open()
                            confirmEidDialog.accepted.connect(function(){
                                var authConfirm = authProxy.signWallet(MobileClient.VerifyWalletKey,  textInputEmail.text, qsTr("Password for wallet %1").arg(tfName.text),
                                                                       seed.walletId, encKey)

                                var confirmProgress = Qt.createComponent("BsControls/BSEidProgressBox.qml").createObject(mainWindow);
                                confirmProgress.title = qsTr("Verify Auth eID signing")
                                confirmProgress.customDetails = qsTr("Wallet ID: %1").arg(seed ? seed.walletId : qsTr(""))
                                confirmProgress.customText = encKey
                                confirmProgress.open()
                                confirmProgress.rejected.connect(function() {
                                    authConfirm.destroy()
                                })

                                authConfirm.succeeded.connect(function(encKey_, password_) {
                                    confirmProgress.acceptAnimated()
                                    authConfirm.destroy()
                                    acceptAnimated()
                                })
                                authConfirm.failed.connect(function(encKey_, password_) {
                                    confirmProgress.rejectAnimated()
                                    authConfirm.destroy()
                                })
                            })
                        })
                        authActivate.failed.connect(function(failed_text) {
                            activateProgress.rejectAnimated()
                            authActivate.destroy()
                        })
                    }
                }
            }
        }
    }

    onAccepted: {
        seed.walletName = tfName.text
        seed.walletDesc = tfDesc.text
        isPrimary = cbPrimary.checked
        if (rbPassword.checked) {
            seed.encType = WalletInfo.Password
            encType = WalletInfo.Password
            password = newPasswordWithConfirm.text
        }
        else {
            seed.encType = WalletInfo.Auth
            seed.encKey = encKey
        }
    }
}
