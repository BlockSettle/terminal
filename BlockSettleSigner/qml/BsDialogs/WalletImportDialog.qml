import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2
import QtQuick.Dialogs 1.2

import com.blocksettle.PasswordConfirmValidator 1.0
import com.blocksettle.QSeed 1.0
import com.blocksettle.WalletInfo 1.0
import com.blocksettle.AuthSignWalletObject 1.0
import com.blocksettle.AutheIDClient 1.0
import com.blocksettle.QPasswordData 1.0
import com.blocksettle.NsWallet.namespace 1.0

import "../BsControls"
import "../StyledControls"
import "../js/helper.js" as JsHelper

CustomTitleDialogWindow {
    id: root

    property bool primaryWalletExists: false
    property string password
    property bool isPrimary: false
    property QSeed seed: QSeed{}
    property WalletInfo walletInfo: WalletInfo{}
    property var passwordData: QPasswordData{}

    property bool acceptable: (curPage === 1 && walletSelected) ||
                              (curPage === 2 && importAcceptable)

    property bool digitalBackupAcceptable: false
    property bool walletSelected: rbPaperBackup.checked ? rootKeyInput.acceptableInput : digitalBackupAcceptable
    property bool importAcceptable: tfName.text.length
                                    && (newPasswordWithConfirm.acceptableInput && rbPassword.checked
                                        || textInputEmail.text && rbAuth.checked)
    property int inputLabelsWidth: 110
    property int curPage: WalletImportDialog.Page.Select
    property bool authNoticeShown: false

    title: qsTr("Import Wallet")
    width: 400
    height: 470
    abortConfirmation: true
    abortBoxType: BSAbortBox.AbortType.WalletImport

    onEnterPressed: {
        if (btnAccept.enabled) btnAccept.onClicked()
    }

    enum Page {
        Select = 1,
        Import = 2
    }

    onWalletInfoChanged: {
        tfName.text = walletInfo.name
        tfDesc.text = walletInfo.desc
    }

    cContentItem: ColumnLayout {
        id: mainLayout
        spacing: 10

        ColumnLayout {
            id: selectLayout
            visible: curPage === WalletImportDialog.Page.Select

            CustomHeader {
                id: headerText
                text: qsTr("Backup Type")
                Layout.fillWidth: true
                Layout.preferredHeight: 25
                Layout.topMargin: 5
                Layout.leftMargin: 10
                Layout.rightMargin: 10
            }

            ColumnLayout {
                id: fullImportLayout

                RowLayout {
                    spacing: 5
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10

                    CustomRadioButton {
                        id: rbPaperBackup
                        Layout.leftMargin: inputLabelsWidth
                        text: qsTr("Paper Backup")
                        checked: true
                    }
                    CustomRadioButton {
                        id: rbFileBackup
                        text: qsTr("Digital Backup")
                    }
                }

                CustomHeader {
                    text: qsTr("Root Private Key")
                    visible: rbPaperBackup.checked
                    Layout.fillWidth: true
                    Layout.preferredHeight: 25
                    Layout.topMargin: 5
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                }
                CustomHeader {
                    text: qsTr("File Location")
                    visible: rbFileBackup.checked
                    Layout.fillWidth: true
                    Layout.preferredHeight: 25
                    Layout.topMargin: 5
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                }

                BSEasyCodeInput {
                    id: rootKeyInput
                    visible: rbPaperBackup.checked
                    rowSpacing: 0
                    columnSpacing: 0
                    Layout.margins: 5
                    //sectionHeaderTxt: qsTr("Enter Root Private Key")
                    line1LabelTxt: qsTr("Root Key Line 1")
                    line2LabelTxt: qsTr("Root Key Line 2")
                    onEntryComplete: {
                        seed = qmlFactory.createSeedFromPaperBackupT(rootKeyInput.privateRootKey, signerSettings.testNet)
                        if (seed.networkType === WalletInfo.Invalid) {
                            JsHelper.messageBoxCritical(qsTr("Error"), qsTr("Failed to parse paper backup key."), "")
                        }
                    }
                }
            }
//            CustomLabel {
//                Layout.fillWidth: true
//                Layout.leftMargin: 10
//                text: qsTr("File Location to Restore")
//                visible: !rbPaperBackup.checked
//            }
            RowLayout {
                spacing: 5
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                Layout.bottomMargin: 10
                visible: !rbPaperBackup.checked

                CustomLabel {
                    Layout.fillWidth: true
                    Layout.minimumWidth: inputLabelsWidth
                    Layout.preferredWidth: inputLabelsWidth
                    Layout.maximumWidth: inputLabelsWidth
                    text: qsTr("Digital backup")
                    verticalAlignment: Text.AlignTop
                    Layout.alignment: Qt.AlignTop
                }

                CustomLabel {
                    id: lblDBFile
                    Layout.fillWidth: true
                    Layout.preferredWidth: 200
                    text: "..."
                    wrapMode: Label.Wrap
                }
                CustomButton {
                    text: qsTr("Select")
                    Layout.alignment: Qt.AlignTop
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
            spacing: 10

            CustomHeader {
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
                    text: seed.walletId
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
                    text: qsTr("Name")
                }
                CustomTextInput {
                    id: tfName
                    selectByMouse: true
                    Layout.fillWidth: true
                    focus: true
                    Keys.onEnterPressed: tfDesc.forceActiveFocus()
                    Keys.onReturnPressed: tfDesc.forceActiveFocus()
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
                    text: qsTr("Description")
                }
                CustomTextInput {
                    id: tfDesc
                    selectByMouse: true
                    Layout.fillWidth: true
                    Keys.onEnterPressed: rbPassword.checked ? newPasswordWithConfirm.tfPasswordInput.forceActiveFocus() : textInputEmail.forceActiveFocus()
                    Keys.onReturnPressed: rbPassword.checked ? newPasswordWithConfirm.tfPasswordInput.forceActiveFocus() : textInputEmail.forceActiveFocus()
                    KeyNavigation.tab: rbPassword.checked ? newPasswordWithConfirm.tfPasswordInput : textInputEmail
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
                    text: qsTr("Primary Wallet")
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
                }
                CustomRadioButton {
                    id: rbAuth
                    text: qsTr("Auth eID")
                    onCheckedChanged: {
                        if (checked) {
                            // show notice dialog
                            if (!signerSettings.hideEidInfoBox) {
                                var noticeEidDialog = Qt.createComponent("../BsControls/BSEidNoticeBox.qml").createObject(mainWindow);
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
                passwordLabelTxt: qsTr("Wallet Password")
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
        }

        Rectangle {
            Layout.fillHeight: true
        }
    }

    cFooterItem: RowLayout {
        CustomButtonBar {
            Layout.fillWidth: true

            CustomButton {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                text: qsTr("Cancel")
                onClicked: {
                    JsHelper.openAbortBox(root, abortBoxType)
                }
            }

            CustomButtonPrimary {
                id: btnAccept
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                text: qsTr("Import")
                enabled: acceptable

                onClicked: {
                    if (curPage === 1) {
                        curPage = 2

                        if (rbPaperBackup.checked) {
                            seed = qmlFactory.createSeedFromPaperBackupT(rootKeyInput.privateRootKey, signerSettings.testNet)
                        }
                        else {
                            seed = qmlFactory.createSeedFromDigitalBackupT(lblDBFile.text, signerSettings.testNet)
                            walletInfo = qmlFactory.createWalletInfoFromDigitalBackup(lblDBFile.text)
                        }
                    }
                    else {
                        walletInfo.name = tfName.text
                        walletInfo.desc = tfDesc.text
                        walletInfo.walletId = seed.walletId
                        walletInfo.rootId = seed.walletId

                        isPrimary = cbPrimary.checked

                        if (rbPassword.checked) {
                            // password
                            passwordData.encType = NsWallet.Password
                            passwordData.encKey = ""
                            passwordData.textPassword = newPasswordWithConfirm.password


                            var checkPasswordDialog = Qt.createComponent("../BsControls/BSPasswordInput.qml").createObject(mainWindow);
                            checkPasswordDialog.passwordToCheck = newPasswordWithConfirm.password
                            checkPasswordDialog.open()
                            checkPasswordDialog.accepted.connect(function(){
                                var ok = walletsProxy.createWallet(isPrimary
                                                               , seed
                                                               , walletInfo
                                                               , passwordData)

                                var mb = JsHelper.resultBox(BSResultBox.WalletImport, ok, walletInfo)
                                if (ok) mb.accepted.connect(acceptAnimated)
                            })
                        }
                        else {
                            // auth eID
                            JsHelper.activateeIdAuth(textInputEmail.text
                                                     , walletInfo
                                                     , function(newPasswordData){
                                                         var ok = walletsProxy.createWallet(isPrimary
                                                                                        , seed
                                                                                        , walletInfo
                                                                                        , newPasswordData)

                                                         var mb = JsHelper.resultBox(BSResultBox.WalletImport, ok, walletInfo)
                                                         if (ok) mb.accepted.connect(acceptAnimated)
                                                     })

                        }

                    }
                }
            }

        }


    }

    Loader {
        id: ldrDBFileDlg
        active: false
        sourceComponent: FileDialog {
            id: dlgDBFile
            visible: false
            title: qsTr("Select Digital Backup file")
            nameFilters: ["Digital Backup files (*.wdb)", "All files (*)"]
            folder: shortcuts.documents

            onAccepted: {
                var filePath = fileUrl.toString()
                if (Qt.platform.os === "windows") {
                    filePath = filePath.replace(/(^file:\/{3})/, "")
                }
                else {
                    filePath = filePath.replace(/(^file:\/{2})/, "") // this might be done like this to work in ubuntu? not sure...
                }
                lblDBFile.text = decodeURIComponent(filePath)
                // seed will be stored to Seed::privKey_
                seed = qmlFactory.createSeedFromDigitalBackupT(lblDBFile.text, signerSettings.testNet)
                walletInfo = qmlFactory.createWalletInfoFromDigitalBackup(lblDBFile.text)

                if (seed.networkType === WalletInfo.Invalid) {
                    digitalBackupAcceptable = false
                    JsHelper.messageBoxCritical(qsTr("Error"), qsTr("Failed to parse digital backup."), qsTr("Path: '%1'").arg(lblDBFile.text))
                }
                else {
                    digitalBackupAcceptable = true
                }
            }
        }
    }
}
