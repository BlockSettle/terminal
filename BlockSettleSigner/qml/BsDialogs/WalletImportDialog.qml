/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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
import "../BsStyles"
import "../BsHw"
import "../js/helper.js" as JsHelper


CustomTitleDialogWindow {
    id: root

    property bool primaryWalletExists: walletsProxy.primaryWalletExists
    // Let's skip hasCCInfo checking, seems to work fine anyway
    //property bool hasCCInfoLoaded: walletsProxy.hasCCInfo
    property bool isPrimaryWalletSelectionVisible: true
    property bool canBePrimary: !primaryWalletExists

    property string password
    property QSeed seed: QSeed{}
    property WalletInfo walletInfo: WalletInfo{}
    property var passwordData: QPasswordData{}
    property bool isWO: (tabBar.currentIndex === 1)

    property bool acceptable: if (isWO)
                                  digitalWoBackupAcceptable
                              else
                                  ((curPage === 1 && walletSelected) ||
                                   (curPage === 2 && importAcceptable))
    property bool importStarted: false

    property bool digitalBackupAcceptable: false
    property bool digitalWoBackupAcceptable: false
    property bool walletSelected: if (rbPaperBackup.checked)
                                      rootKeyInput.acceptableInput
                                  else if(rbBip39_12.checked || rbBip39_24.checked)
                                      rootKeyInputBip39.accepted
                                  else
                                      digitalBackupAcceptable
    property bool importAcceptable: tfName.text.length
                                    && (newPasswordWithConfirm.acceptableInput && rbPassword.checked
                                        || rbAuth.checked)
    property int inputLabelsWidth: 110
    property int curPage: WalletImportDialog.Page.Select
    property bool authNoticeShown: false

    title: qsTr("Import Wallet")
    width: 540
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
                    Layout.fillWidth: true

                    CustomHeader {
                        id: headerText
                        text: qsTr("Wallet Type")
                        Layout.fillWidth: true
                        Layout.preferredHeight: 25
                        Layout.topMargin: 5
                        Layout.leftMargin: 10
                        Layout.rightMargin: 10
                    }

                    ColumnLayout {
                        id: fullImportLayout
                        Layout.fillWidth: true
                        RowLayout {
                            id: radioButtonsParent

                            spacing: 5
                            Layout.fillWidth: true
                            Layout.leftMargin: 10
                            Layout.rightMargin: 10


                            ColumnLayout {
                                Layout.leftMargin: 5
                                spacing: 1

                                CustomLabel {
                                    text: "BlockSettle"
                                    Layout.alignment: Qt.AlignLeft
                                }

                                RowLayout {
                                    CustomRadioButton {
                                        id: rbPaperBackup
                                        text: qsTr("Paper Backup")
                                        checked: true
                                        onClicked: radioButtonsParent.checkedChanged(this)
                                    }
                                    CustomRadioButton {
                                        id: rbFileBackup
                                        text: qsTr("Digital Backup")
                                        onClicked: radioButtonsParent.checkedChanged(this)
                                    }
                                }
                            }

                            Rectangle {
                                height: rbFileBackup.height
                                width: 1
                                color: Qt.rgba(1, 1, 1, 0.1)
                                Layout.alignment: Qt.AlignBottom
                            }

                            ColumnLayout {
                                spacing: 1

                                CustomLabel {
                                    text:  qsTr("Bip39")
                                    Layout.alignment: Qt.AlignLeft
                                }

                                RowLayout {
                                    CustomRadioButton {
                                        id: rbBip39_12
                                        text: qsTr("12 word seed")
                                        onClicked: radioButtonsParent.checkedChanged(this)
                                    }
                                    CustomRadioButton {
                                        id: rbBip39_24
                                        text: qsTr("24 word seed")
                                        onClicked: radioButtonsParent.checkedChanged(this)
                                    }
                                }
                            }

                            function checkedChanged(rbutton) {
                                rbPaperBackup.checked = (rbutton === rbPaperBackup);
                                rbFileBackup.checked = (rbutton === rbFileBackup);
                                rbBip39_12.checked = (rbutton === rbBip39_12);
                                rbBip39_24.checked = (rbutton === rbBip39_24);

                                chkImportLegacy.checked = rbBip39_12.checked || rbBip39_24.checked

                                canBePrimary = !(primaryWalletExists || chkImportLegacy.checked)
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
                            line1LabelTxt: qsTr("Line 1")
                            line2LabelTxt: qsTr("Line 2")
                            onEntryComplete: {
                                seed = qmlFactory.createSeedFromPaperBackupT(rootKeyInput.privateRootKey, signerSettings.testNet)
                                if (seed.networkType === WalletInfo.Invalid) {
                                    JsHelper.messageBoxCritical(qsTr("Error"), qsTr("Failed to parse paper backup key."), "")
                                }
                            }
                        }

                        BSBip39Input {
                            id: rootKeyInputBip39
                            visible: rbBip39_24.checked || rbBip39_12.checked
                            wordsCount: rbBip39_12.checked ? 12 : 24
                            Layout.fillWidth: true
                            Layout.leftMargin: 5
                            Layout.rightMargin: 10
                            onEntryComplete: {
                                seed = qmlFactory.createSeedFromMnemonic(mnemonicSentence, signerSettings.testNet)
                            }
                        }
                    }

                    RowLayout {
                        spacing: 5
                        Layout.fillWidth: true
                        Layout.leftMargin: 10
                        Layout.rightMargin: 10
                        Layout.bottomMargin: 10
                        visible: rbFileBackup.checked

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

                    Item {
                        // spacer item
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                    }

                    RowLayout {
                        spacing: 5
                        Layout.fillWidth: true
                        Layout.leftMargin: 10
                        Layout.rightMargin: 10


                        CustomCheckBox {
                            id: chkImportLegacy
                            text: qsTr("Import legacy path (BIP44)")
                            checked: false

                            onClicked: {
                                canBePrimary = !(primaryWalletExists || checked)
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
                            Layout.fillWidth: true
                            Layout.minimumWidth: inputLabelsWidth
                            Layout.preferredWidth: inputLabelsWidth
                            Layout.maximumWidth: inputLabelsWidth
                            text: qsTr("Description (optional)")
                        }
                        CustomTextInput {
                            id: tfDesc
                            selectByMouse: true
                            Layout.fillWidth: true
                            Keys.onEnterPressed: rbPassword.checked ? newPasswordWithConfirm.tfPasswordInput.forceActiveFocus() : null
                            Keys.onReturnPressed: rbPassword.checked ? newPasswordWithConfirm.tfPasswordInput.forceActiveFocus() : null
                            KeyNavigation.tab: rbPassword.checked ? newPasswordWithConfirm.tfPasswordInput : null
                        }
                    }

                    CustomHeader {
                        visible: isPrimaryWalletSelectionVisible
                        text: qsTr("Primary Wallet")
                        Layout.fillWidth: true
                        Layout.preferredHeight: 25
                        Layout.topMargin: 5
                        Layout.leftMargin: 10
                        Layout.rightMargin: 10
                    }

                    RowLayout {
                        visible: isPrimaryWalletSelectionVisible
                        spacing: 5
                        Layout.alignment: Qt.AlignTop
                        Layout.fillWidth: true
                        Layout.leftMargin: 10
                        Layout.rightMargin: 10

                        CustomLabel {
                            visible: isPrimaryWalletSelectionVisible
                            Layout.minimumWidth: inputLabelsWidth
                            Layout.preferredWidth: inputLabelsWidth
                            Layout.maximumWidth: inputLabelsWidth
                            Layout.fillWidth: true
                            text: qsTr("Primary Wallet")
                        }

                        CustomCheckBox {
                            id: cbPrimary
                            visible: isPrimaryWalletSelectionVisible
                            Layout.fillWidth: true
                            checked: canBePrimary
                            text: qsTr("Primary Wallet")

                            ToolTip.text: { canBePrimary
                                ? qsTr("A primary wallet does not exist, wallet will be created as primary.")
                                : (primaryWalletExists
                                            ? qsTr("A primary wallet already exists, wallet will be created as regular wallet.")
                                            :  qsTr("This wallet could not be imported as primary."))}

                            ToolTip.delay: 150
                            ToolTip.timeout: 5000
                            ToolTip.visible: cbPrimary.hovered

                            // workaround on https://bugreports.qt.io/browse/QTBUG-30801
                            // enabled: !primaryWalletExists
                            onCheckedChanged: {
                                // BST-2411: first wallet is always created as primary
                                cbPrimary.checked = canBePrimary;
                            }
                        }
                    }

                    CustomHeader {
                        id: headerText2
                        text: qsTr("Encrypt your Wallet")
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
                                    newPasswordWithConfirm.tfPasswordInput.forceActiveFocus()
                                }
                            }
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
                enabled: !importStarted
                onClicked: {
                    JsHelper.openAbortBox(root, abortBoxType)
                }
            }

            CustomButton {
                id: btnAccept
                primary: true
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                text: qsTr("Import")
                enabled: acceptable && !importStarted

                onClicked: {
                    if (isWO) {
                        importWoWallet();
                        return;
                    }

                    if (curPage === 1) {
                        if (!(primaryWalletExists || chkImportLegacy.checked)) {
                            cbPrimary.checked = true
                            cbPrimary.enabled = false
                            tfName.text = qsTr("Primary Wallet");
                        } else {
                            cbPrimary.checked = false
                            tfName.text = walletsProxy.generateNextWalletName();
                        }

                        curPage = 2
                        tfName.forceActiveFocus()
                        tfName.selectAll()

                        if (rbPaperBackup.checked) {
                            seed = qmlFactory.createSeedFromPaperBackupT(rootKeyInput.privateRootKey, signerSettings.testNet)
                        }
                        else if (rbBip39_24.checked || rbBip39_12.checked) {
                            seed = qmlFactory.createSeedFromMnemonic(rootKeyInputBip39.mnemonicSentence, signerSettings.testNet)
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
                        walletInfo.encType = QPasswordData.Password

                        var createCallback = function(success, errorMsg) {
                            importStarted = false
                            if (success) {
                                var mb = JsHelper.resultBox(BSResultBox.ResultType.WalletImport, true, walletInfo)
                                mb.bsAccepted.connect(acceptAnimated)
                            }
                            else {
                                JsHelper.messageBox(BSMessageBox.Type.Critical
                                    , qsTr("Import Failed"), qsTr("Import wallet failed with error: \n") + errorMsg)
                            }
                        }
                        var failedCallback = function() {
                            importStarted = false
                        }

                        importStarted = true

                        if (rbPassword.checked) {
                            // password
                            passwordData.encType = QPasswordData.Password
                            passwordData.encKey = ""
                            passwordData.textPassword = newPasswordWithConfirm.password

                            var checkPasswordDialog = Qt.createComponent("../BsControls/BSPasswordInputConfirm.qml").createObject(mainWindow,
                               {"walletInfo": walletInfo})
                            checkPasswordDialog.passwordToCheck = newPasswordWithConfirm.password
                            checkPasswordDialog.open()
                            checkPasswordDialog.bsAccepted.connect(function() {
                                walletsProxy.createWallet(cbPrimary.checked, chkImportLegacy.checked, seed, walletInfo, passwordData, createCallback)
                            })
                            checkPasswordDialog.bsRejected.connect(failedCallback)
                        }
                        else {
                            // auth eID
                            let authEidMessage = JsHelper.getAuthEidWalletInfo(walletInfo);
                            var successCallback = function(newPasswordData) {
                                walletsProxy.createWallet(cbPrimary.checked, chkImportLegacy.checked, seed, walletInfo, newPasswordData, createCallback)
                            }
                            JsHelper.activateeIdAuth(walletInfo, authEidMessage, successCallback, failedCallback)
                        }
                    }
                }
            }
        }
    }

    function applyDialogClosing() {
        JsHelper.openAbortBox(root, abortBoxType);
        return false;
    }

    function importWoWallet() {
        var importCallback = function(success, id, name, desc) {
            if (success) {
                let walletInfo = qmlFactory.createWalletInfo();
                walletInfo.walletId = id;
                walletInfo.name = name;
                walletInfo.desc = desc;

                let type = BSResultBox.ResultType.WalletImportWo;

                var mb = JsHelper.resultBox(type, true, walletInfo)
                mb.bsAccepted.connect(acceptAnimated)
            }
            else {
                JsHelper.messageBox(BSMessageBox.Type.Critical
                    , qsTr("Import Failed"), qsTr("Import Watching Only Wallet failed:\n") + desc)
            }
        }

        if (isWO) {
            walletsProxy.importWoWallet(lblWoDBFile.text, importCallback)
        }
    }
}
