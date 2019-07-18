import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.4
import com.blocksettle.PasswordConfirmValidator 1.0
import com.blocksettle.WalletsProxy 1.0
import com.blocksettle.AuthSignWalletObject 1.0
import com.blocksettle.AutheIDClient 1.0

import com.blocksettle.WalletInfo 1.0
import com.blocksettle.QSeed 1.0
import com.blocksettle.QPasswordData 1.0


import "../BsControls"
import "../BsStyles"
import "../StyledControls"
import "../js/helper.js" as JsHelper

CustomTitleDialogWindow {
    id: root

    property bool primaryWalletExists: walletsProxy.primaryWalletExists

    property int inputLabelsWidth: 110

    property var walletInfo: WalletInfo{}
    property var seed
    property var passwordData: QPasswordData{}

    width: 400
    height: 470
    abortConfirmation: true
    abortBoxType: BSAbortBox.AbortType.WalletCreation
    title: qsTr("Manage encryption")

    Component.onCompleted: {
        if (!primaryWalletExists) {
            cbPrimary.checked = true
            tfName.text = qsTr("Primary Wallet");
        }
        else {
            tfName.text = walletsProxy.generateNextWalletName();
        }
    }

    cContentItem: ColumnLayout {
        spacing: 10

        CustomHeader {
            id: headerText
            text: qsTr("Wallet Details")
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

            CustomLabel {
                Layout.minimumWidth: inputLabelsWidth
                Layout.preferredWidth: inputLabelsWidth
                Layout.maximumWidth: inputLabelsWidth
                Layout.fillWidth: true
                text: qsTr("Wallet ID")
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
                text: qsTr("Name")
            }
            CustomTextInput {
                id: tfName
                Layout.fillWidth: true
                selectByMouse: true
                focus: true
                color: walletsProxy.walletNameExists(tfName.text) ? BSStyle.inputsInvalidColor : BSStyle.inputsFontColor
                Keys.onEnterPressed: tfDesc.forceActiveFocus()
                Keys.onReturnPressed: tfDesc.forceActiveFocus()
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
                text: qsTr("Description")
            }
            CustomTextInput {
                id: tfDesc
                Layout.fillWidth: true
                selectByMouse: true
                validator: RegExpValidator {
                    regExp: /^[^\\\\/?:*<>|]*$/
                }
                Keys.onEnterPressed: rbPassword.checked ? newPasswordWithConfirm.tfPasswordInput.forceActiveFocus() : textInputEmail.forceActiveFocus()
                Keys.onReturnPressed: rbPassword.checked ? newPasswordWithConfirm.tfPasswordInput.forceActiveFocus() : textInputEmail.forceActiveFocus()
                KeyNavigation.tab: rbPassword.checked ? newPasswordWithConfirm.tfPasswordInput : textInputEmail
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
                text: qsTr("Primary Wallet")
                checked: !primaryWalletExists

                ToolTip.text: qsTr("A primary Wallet already exists, wallet will be created as regular wallet.")
                ToolTip.delay: 150
                ToolTip.timeout: 5000
                ToolTip.visible: cbPrimary.hovered && primaryWalletExists

                // workaround on https://bugreports.qt.io/browse/QTBUG-30801
                // enabled: !primaryWalletExists
                onCheckedChanged: {
                    if (primaryWalletExists) cbPrimary.checked = false;

                    if (!primaryWalletExists && (tfName.text === walletsProxy.generateNextWalletName() || tfName.text.length === 0)) {
                        tfName.text = qsTr("Primary Wallet");
                    }
                    else if (tfName.text === qsTr("Primary Wallet")  || tfName.text.length === 0){
                        tfName.text = walletsProxy.generateNextWalletName();
                    }
                }
            }
        }
        CustomHeader {
            id: headerText2
            text: qsTr("Encryption")
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

                onCheckedChanged: {
                    if (checked) {
                        newPasswordWithConfirm.tfPasswordInput.focus = true
                    }

                    if (!primaryWalletExists && tfName.text === walletsProxy.generateNextWalletName()) {
                        tfName.text = qsTr("Primary Wallet");
                    }
                    else if (tfName.text === qsTr("Primary Wallet")){
                        tfName.text = walletsProxy.generateNextWalletName();
                    }
                }
            }
            CustomRadioButton {
                id: rbAuth
                text: qsTr("Auth eID")

                onCheckedChanged: {
                    if (checked) {
                        textInputEmail.focus = true
                        // show notice dialog
                        if (!signerSettings.hideEidInfoBox) {
                            var noticeEidDialog = Qt.createComponent("../BsControls/BSEidNoticeBox.qml").createObject(mainWindow);
                            sizeChanged(noticeEidDialog.width, noticeEidDialog.height)

                            noticeEidDialog.closed.connect(function(){
                                sizeChanged(root.width, root.height)
                            })
                            noticeEidDialog.open()
                        }
                    }
                }
            }
        }

        BSConfirmedPasswordInput {
            id: newPasswordWithConfirm
            visible: rbPassword.checked
            columnSpacing: 10
            passwordLabelTxt: qsTr("Password")
            confirmLabelTxt: qsTr("Confirm Password")
            onConfirmInputEnterPressed: {
                if (btnAccept.enabled) btnAccept.onClicked()
            }
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
                text: qsTr("Auth eID email")
            }
            CustomTextInput {
                id: textInputEmail
                Layout.fillWidth: true
                selectByMouse: true
                focus: true
                Keys.onEnterPressed: {
                    if (btnAccept.enabled) btnAccept.onClicked()
                }
                Keys.onReturnPressed: {
                    if (btnAccept.enabled) btnAccept.onClicked()
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
                id: btnAccept
                text: qsTr("Continue")
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                enabled: !walletsProxy.walletNameExists(tfName.text)
                            && tfName.text.length
                            && (newPasswordWithConfirm.acceptableInput && rbPassword.checked ||
                                textInputEmail.text && rbAuth.checked)

                onClicked: {
                    walletInfo.name = tfName.text
                    walletInfo.desc = tfDesc.text
                    walletInfo.walletId = seed.walletId
                    walletInfo.rootId = seed.walletId

                    var createCallback = function(success, errorMsg) {
                        if (success) {
                            var mb = JsHelper.resultBox(BSResultBox.ResultType.WalletCreate, true, walletInfo)
                            mb.bsAccepted.connect(acceptAnimated)
                        } else {
                            JsHelper.messageBox(BSMessageBox.Type.Critical
                                , qsTr("Import Failed"), qsTr("Create wallet failed with error: \n") + errorMsg)
                        }
                    }

                    if (rbPassword.checked) {
                        // auth password
                        var checkPasswordDialog = Qt.createComponent("../BsControls/BSPasswordInput.qml").createObject(mainWindow);
                        checkPasswordDialog.passwordToCheck = newPasswordWithConfirm.password
                        checkPasswordDialog.type = BSPasswordInput.Type.Confirm
                        checkPasswordDialog.open()
                        checkPasswordDialog.bsAccepted.connect(function(){
                            passwordData.encType = QPasswordData.Password
                            passwordData.encKey = ""
                            passwordData.textPassword = newPasswordWithConfirm.password
                            walletsProxy.createWallet(cbPrimary.checked, seed, walletInfo, passwordData, createCallback)
                        })
                    }
                    else {
                        // auth eID
                        JsHelper.activateeIdAuth(textInputEmail.text, walletInfo, function(newPasswordData) {
                            walletsProxy.createWallet(cbPrimary.checked, seed, walletInfo, newPasswordData, createCallback)
                        })
                    }
                }
            }
        }
    }
}
