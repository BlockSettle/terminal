import QtQuick 2.9
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.2
import Qt.labs.platform 1.1

import com.blocksettle.PasswordConfirmValidator 1.0
import com.blocksettle.QSeed 1.0
import com.blocksettle.WalletInfo 1.0
import com.blocksettle.AuthSignWalletObject 1.0
import com.blocksettle.AutheIDClient 1.0
import com.blocksettle.QPasswordData 1.0

import "../BsControls"
import "../StyledControls"
import "../js/helper.js" as JsHelper

CustomTitleDialogWindow {
    id: root

    property bool primaryWalletExists: walletsProxy.primaryWalletExists
    property string password
    property QSeed seed: QSeed{}
    property WalletInfo walletInfo: WalletInfo{}
    property var passwordData: QPasswordData{}
    property bool isWO: (tabBar.currentIndex === 1)

    property bool acceptable: isWO ? digitalWoBackupAcceptable : ((curPage === 1 && walletSelected) ||
                              (curPage === 2 && importAcceptable))

    property bool digitalBackupAcceptable: false
    property bool digitalWoBackupAcceptable: false
    property bool walletSelected: rbPaperBackup.checked ? rootKeyInput.acceptableInput : digitalBackupAcceptable
    property bool importAcceptable: tfName.text.length
                                    && (newPasswordWithConfirm.acceptableInput && rbPassword.checked
                                        || textInputEmail.text && rbAuth.checked)
    property int inputLabelsWidth: 110
    property int curPage: WalletImportDialog.Page.Select
    property bool authNoticeShown: false

    title: qsTr("Import Wallet")
    width: 400
    height: 510
    abortConfirmation: true
    abortBoxType: BSAbortBox.AbortType.WalletImport

    Component.onCompleted: {
        if (!primaryWalletExists) {
            cbPrimary.checked = true
        }
    }

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
            TabBar {
                id: tabBar
                spacing: 0
                leftPadding: 1
                rightPadding: 1
                height: 35

                Layout.fillWidth: true
                position: TabBar.Header

                background: Rectangle {
                    anchors.fill: parent
                    color: "transparent"
                }

                CustomTabButton {
                    id: fullImportTabButton
                    text: "Full"
                    cText.font.capitalization: Font.MixedCase
                    implicitHeight: 35
                }
                CustomTabButton {
                    id: woImportTabButton
                    text: "Watch-Only"
                    cText.font.capitalization: Font.MixedCase
                    implicitHeight: 35
                }
            }
        }

        StackLayout {
            id: stackView
            currentIndex: tabBar.currentIndex
            Layout.fillWidth: true

            ColumnLayout {
                id: fullImportTab
                spacing: 5
                Layout.topMargin: 15
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                Layout.fillWidth: true

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
                            line1LabelTxt: qsTr("Line 1")
                            line2LabelTxt: qsTr("Line 2")
                            onEntryComplete: {
                                seed = qmlFactory.createSeedFromPaperBackupT(rootKeyInput.privateRootKey, signerSettings.testNet)
                                if (seed.networkType === WalletInfo.Invalid) {
                                    JsHelper.messageBoxCritical(qsTr("Error"), qsTr("Failed to parse paper backup key."), "")
                                }
                            }
                        }
                    }

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
                            onClicked: dlgDBFile.open()

                            FileDialog {
                                id: dlgDBFile
                                title: qsTr("Select Digital Backup file")
                                nameFilters: ["Digital Backup files (*.wdb)", "All files (*)"]
                                folder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)

                                onAccepted: {
                                    let filePath = qmlAppObj.getUrlPath(file)
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
                            checked: !primaryWalletExists
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

                            onCheckedChanged: {
                                if (checked) {
                                    newPasswordWithConfirm.tfPasswordInput.focus = true
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

            ColumnLayout {
                id: woImportTab

                spacing: 5
                Layout.topMargin: 15
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                Layout.fillWidth: true

                CustomHeader {
                    text: qsTr("File Location")
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
                    Layout.bottomMargin: 10

                    CustomLabel {
                        Layout.fillWidth: true
                        Layout.minimumWidth: inputLabelsWidth
                        Layout.preferredWidth: inputLabelsWidth
                        Layout.maximumWidth: inputLabelsWidth
                        text: qsTr("Watching-Only Wallet file")
                        verticalAlignment: Text.AlignTop
                        Layout.alignment: Qt.AlignTop
                    }

                    CustomLabel {
                        id: lblWoDBFile
                        Layout.fillWidth: true
                        Layout.preferredWidth: 200
                        text: "..."
                        wrapMode: Label.Wrap
                    }
                    CustomButton {
                        text: qsTr("Select")
                        Layout.alignment: Qt.AlignTop
                        onClicked: dlgWoDBFile.open()

                        FileDialog {
                            id: dlgWoDBFile
                            title: qsTr("Select Watching-Only Wallet Backup file")
                            nameFilters: ["Watching-Only Backup files (*.lmdb)", "All files (*)"]
                            folder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)

                            onAccepted: {
                                let filePath = qmlAppObj.getUrlPath(file)
                                lblWoDBFile.text = decodeURIComponent(filePath)

                                digitalWoBackupAcceptable = true
                            }
                        }
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
                    if (isWO) {
                        var importCallback = function(success, msg) {
                            if (success) {
                                var walletInfo = qmlFactory.createWalletInfo(msg)
                                var mb = JsHelper.resultBox(BSResultBox.ResultType.WalletImportWo, true, walletInfo)
                                mb.bsAccepted.connect(acceptAnimated)
                            } else {
                                JsHelper.messageBox(BSMessageBox.Type.Critical
                                    , qsTr("Import Failed"), qsTr("Import WO-wallet failed:\n") + msg)
                            }
                        }

                        walletsProxy.importWoWallet(lblWoDBFile.text, importCallback)
                        return
                    }

                    if (curPage === 1) {
                        curPage = 2
                        tfName.forceActiveFocus()
                        tfName.selectAll()

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

                        var createCallback = function(success, errorMsg) {
                            if (success) {
                                var mb = JsHelper.resultBox(BSResultBox.ResultType.WalletImport, true, walletInfo)
                                mb.bsAccepted.connect(acceptAnimated)
                            } else {
                                JsHelper.messageBox(BSMessageBox.Type.Critical
                                    , qsTr("Import Failed"), qsTr("Import wallet failed with error: \n") + errorMsg)
                            }
                        }

                        if (rbPassword.checked) {
                            // password
                            passwordData.encType = QPasswordData.Password
                            passwordData.encKey = ""
                            passwordData.textPassword = newPasswordWithConfirm.password

                            var checkPasswordDialog = Qt.createComponent("../BsControls/BSPasswordInput.qml").createObject(mainWindow);
                            checkPasswordDialog.passwordToCheck = newPasswordWithConfirm.password
                            checkPasswordDialog.type = BSPasswordInput.Type.Confirm
                            checkPasswordDialog.open()
                            checkPasswordDialog.bsAccepted.connect(function() {
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
}
