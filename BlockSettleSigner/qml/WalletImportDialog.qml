import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2
import QtQuick.Dialogs 1.2
import com.blocksettle.PasswordConfirmValidator 1.0
import com.blocksettle.WalletSeed 1.0
import com.blocksettle.WalletInfo 1.0
import com.blocksettle.AuthProxy 1.0
import com.blocksettle.AuthSignWalletObject 1.0
import com.blocksettle.AutheIDClient 1.0

import "BsControls"
import "StyledControls"
import "js/helper.js" as JsHelper

CustomTitleDialogWindow {
    id: root

    property bool primaryWalletExists: false
    property string password
    property bool isPrimary: false
    property WalletSeed seed
    property int encType
    property string encKey
    property AuthSignWalletObject  authSign
    property bool acceptable: (curPage === 1 && walletSelected) ||
                              (curPage === 2 && importAcceptable)

    property bool walletSelected: rbPaperBackup.checked ? rootKeyInput.acceptableInput : lblDBFile.text !== "..."
    property bool importAcceptable: tfName.text.length &&
                                    (newPasswordWithConfirm.acceptableInput && rbPassword.checked ||
                                     textInputEmail.text && rbAuth.checked)
    property int inputLabelsWidth: 105
    property int curPage: WalletImportDialog.Page.Select
    property bool authNoticeShown: false

    title: qsTr("Import Wallet")

    enum Page {
        Select = 1,
        Import = 2
    }

    Connections {
        target: seed
        onError: {
            JsHelper.messageBoxCritical(qsTr("Error"), qsTr("Failed to parse backup."), qsTr("Error: %1").arg(errMsg))
        }
    }

    ButtonGroup {
        id: btnGroup
    }

    cContentItem: ColumnLayout {
        id: mainLayout
        spacing: 10
        anchors.fill: root

        ColumnLayout {
            id: selectLayout
            visible: curPage === WalletImportDialog.Page.Select

            RowLayout {
                visible: false // WO wallet cannot be imported in signer
                spacing: 0
                Layout.fillWidth: true
                Layout.margins: 10

                CustomButton {
                    id: btnFull
                    implicitWidth: 90
                    implicitHeight: 20
                    text: qsTr("Full")
                    capitalize: false
                    checkable: true
                    ButtonGroup.group: btnGroup
                    checked: true
                }
                CustomButton {
                    id: btnWatchOnly
                    implicitWidth: 90
                    implicitHeight: 20
                    text: qsTr("Watch-Only")
                    capitalize: false
                    checkable: true
                    ButtonGroup.group: btnGroup
                }
            }

            ColumnLayout {
                id: fullImportLayout
                visible: !btnWatchOnly.checked

                CustomLabel {
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    text: qsTr("Backup Type")
                }
                RowLayout {
                    spacing: 5
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10

                    CustomRadioButton {
                        id: rbPaperBackup
                        text:   qsTr("Paper Backup")
                        checked:    true
                    }
                    CustomRadioButton {
                        id: rbFileBackup
                        text:   qsTr("Digital Backup File")
                    }
                }

                BSEasyCodeInput {
                    id: rootKeyInput
                    visible: rbPaperBackup.checked
                    rowSpacing: 0
                    columnSpacing: 0
                    Layout.margins: 5
                    sectionHeaderTxt: qsTr("Enter Root Private Key")
                    line1LabelTxt: qsTr("Root Key Line 1")
                    line2LabelTxt: qsTr("Root Key Line 2")
                    onEntryComplete: {
                        if (!seed.parsePaperKey(privateRootKey)) {
                            ibFailure.displayMessage("Failed to parse paper backup key")
                        }
                    }
                }
            }
            CustomLabel {
                Layout.fillWidth: true
                Layout.leftMargin: 10
                text: qsTr("File Location to Restore")
                visible: !rbPaperBackup.checked || btnWatchOnly.checked
            }
            RowLayout {
                spacing: 5
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                Layout.bottomMargin: 10
                visible: !rbPaperBackup.checked || btnWatchOnly.checked

                CustomLabel {
                    Layout.fillWidth: true
                    Layout.minimumWidth: inputLabelsWidth
                    Layout.preferredWidth: inputLabelsWidth
                    Layout.maximumWidth: inputLabelsWidth
                    text: qsTr("Digital backup")
                }

                CustomLabel {
                    id:     lblDBFile
                    Layout.fillWidth: true
                    text:   "..."
                    wrapMode: Label.Wrap
                    Layout.preferredWidth: 200
                }
                CustomButton {
                    text:   qsTr("Select")
                    onClicked: {
                        if (!ldrDBFileDlg.item) {
                            ldrDBFileDlg.active = true;
                        }
                        ldrDBFileDlg.item.open();
                    }
                }
            }
        }

        ColumnLayout {
            id: importLayout
            visible: curPage === WalletImportDialog.Page.Import

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
                    Layout.fillWidth: true
                    Layout.minimumWidth: inputLabelsWidth
                    Layout.preferredWidth: inputLabelsWidth
                    Layout.maximumWidth: inputLabelsWidth
                    text:   qsTr("Wallet Name")
                }
                CustomTextInput {
                    id: tfName
                    selectByMouse: true
                    Layout.fillWidth: true
                    focus: true
                    onEditingFinished: {
                        seed.walletName = tfName.text
                    }
                }
            }

            RowLayout {
                visible: true
                spacing: 5
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10

                CustomLabel {
                    Layout.fillWidth: true
                    Layout.minimumWidth: inputLabelsWidth
                    Layout.preferredWidth: inputLabelsWidth
                    Layout.maximumWidth: inputLabelsWidth
                    text:   qsTr("Wallet Description")
                }
                CustomTextInput {
                    id: tfDesc
                    selectByMouse: true
                    Layout.fillWidth: true
                    onEditingFinished: {
                        seed.walletDesc = tfDesc.text
                    }
                }
            }
            RowLayout {
                spacing: 5
                Layout.alignment: Qt.AlignTop
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
                visible: rbPassword.checked
                columnSpacing: 10
                rowSpacing: 0
                passwordLabelTxt: qsTr("Wallet Password")
                passwordInputPlaceholder: qsTr("New Wallet Password")
                confirmLabelTxt: qsTr("Confirm Password")
                confirmInputPlaceholder: qsTr("Confirm New Wallet Password")
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
                    onEditingFinished: {
                        seed.walletName = tfName.text
                    }
                }
            }
        }

    }

    cFooterItem: RowLayout {
        CustomButtonBar {
            Layout.fillWidth: true

            CustomButton {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                text:   qsTr("Cancel")
                onClicked: {
                    JsHelper.openAbortBox(root, abortBoxType)
                }
            }

            CustomButtonPrimary {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                text:   qsTr("Import")
                enabled: acceptable

                onClicked: {
                    if (curPage === 1) {
                        curPage = 2
                    }
                    else {
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

                            var authActivate = authProxy.signWallet(AutheIDClient.ActivateWallet,  textInputEmail.text, qsTr("Password for wallet %1").arg(tfName.text),
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
                                activateProgress.acceptAnimated()
                                authActivate.destroy()

                                encKey = encKey_
                                seed.password = password_
                                //acceptable = true

                                var confirmEidDialog = Qt.createComponent("BsControls/BSEidConfirmBox.qml").createObject(mainWindow);
                                confirmEidDialog.open()
                                confirmEidDialog.accepted.connect(function(){
                                    var authConfirm = authProxy.signWallet(AutheIDClient.VerifyWalletKey,  textInputEmail.text, qsTr("Password for wallet %1").arg(tfName.text),
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


    }

    onAccepted: {
        if (rbPassword.checked) {
            password = newPasswordWithConfirm.text
            seed.encType = WalletInfo.Password
            encType = WalletInfo.Password
        }
        else {
            seed.encType = WalletInfo.Auth
            seed.encKey = encKey
        }

        isPrimary = cbPrimary.checked
    }

    onRejected: {
        authSign.cancel()
    }

    Loader {
        id:     ldrDBFileDlg
        active: false
        sourceComponent: FileDialog {
            id:             dlgDBFile
            visible:        false
            title:          qsTr("Select Digital Backup file")
            nameFilters:    ["Digital Backup files (*.wdb)", "All files (*)"]
            folder:         shortcuts.documents

            onAccepted: {
                var filePath = fileUrl.toString()
                console.log(filePath)
                if (Qt.platform.os === "windows") {
                    filePath = filePath.replace(/(^file:\/{3})/, "")
                }
                else {
                    filePath = filePath.replace(/(^file:\/{2})/, "") // this might be done like this to work in ubuntu? not sure...
                }
                lblDBFile.text = decodeURIComponent(filePath)
                if (!seed.parseDigitalBackupFile(lblDBFile.text)) {
                    lblDBFile.text = "..."
                    // not showing a message box here because seed with send a signal with error message which will be shown anyways
                    //ibFailure.displayMessage(qsTr("Failed to parse digital backup from %1").arg(lblDBFile.text))
                    //messageBoxCritical(qsTr("Error"), qsTr("Failed to parse digital backup."), qsTr("Path: '%1'").arg(lblDBFile.text))
                } else if (tfName.text.length === 0) {
                    tfName.text = seed.walletName
                }
            }
        }
    }
}
