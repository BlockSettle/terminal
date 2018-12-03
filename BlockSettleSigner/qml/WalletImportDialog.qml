import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2
import QtQuick.Dialogs 1.2
import com.blocksettle.PasswordConfirmValidator 1.0
import com.blocksettle.WalletSeed 1.0
import com.blocksettle.WalletInfo 1.0
import com.blocksettle.AuthProxy 1.0
import com.blocksettle.AuthSignWalletObject 1.0

import "bscontrols"

CustomDialog {
    id: root
    implicitWidth: 400
    implicitHeight: mainLayout.implicitHeight
    property bool primaryWalletExists: false
    property string password
    property bool isPrimary: false
    property WalletSeed seed
    property int encType
    property string encKey
    property AuthSignWalletObject  authSign
    property bool acceptable: (curPage === 1 && walletSelected) || (curPage === 2 && importAcceptable)
    property bool walletSelected: rbPaperBackup.checked ? rootKeyInput.acceptableInput : lblDBFile.text !== "..."
    property bool importAcceptable: tfName.text.length && (confirmedPassworrdInput.acceptableInput || password.length)
    property int inputLabelsWidth: 105
    property int curPage: WalletImportDialog.Page.Select
    property bool authNoticeShown: false

    enum Page {
        Select = 1,
        Import = 2
    }

    Connections {
        target: seed
        onError: {
            messageBoxCritical(qsTr("Error"), qsTr("Failed to parse backup."), qsTr("Error: %1").arg(errMsg))
        }
    }

    ButtonGroup {
        id: btnGroup
    }

    // this function is called by abort message box in WalletsPage
    function abort() {
        reject()
    }

    // handles accept signal from msgBox which displays password and auth eid notice
    function msgBoxAccept() {
        // accept only when using passwort authentication
        if (rbPassword.checked) {
                accept()
            }
    }

    onOpened: {
        abortBox.bWalletCreate = false
        abortBox.accepted.connect(abort)
        noticeBox.accepted.connect(msgBoxAccept)
    }
    onClosed: {
        abortBox.accepted.disconnect(abort)
        noticeBox.accepted.disconnect(msgBoxAccept)
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
            id: mainLayout
            anchors.fill: parent

            CustomHeaderPanel{
                id: labelTitle
                Layout.preferredHeight: 40
                Layout.fillWidth: true
                text: qsTr("Import Wallet")
            }

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
                            if (checked == true && !authNoticeShown) {
                                // setting to true and then false to properly size
                                // the message box, otherwise sometimes the size is not correct
                                noticeBox.passwordNotice = true
                                noticeBox.passwordNotice = false
                                noticeBox.open()
                                authNoticeShown = true // make sure the notice is only shown once
                            }
                        }
                    }
                }

                BSConfirmedPasswordInput {
                    id: confirmedPassworrdInput
                    visible:    rbPassword.checked
                    columnSpacing: 10
                    rowSpacing: 0
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
                            password = ''
                            authSign = auth.signWallet(tiAuthId.text, qsTr("Password for wallet %1").arg(tfName.text),
                                                                  seed.walletId)
                            btnAuth.enabled = false
                            authSign.success.connect(function(key) {
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
                        text:   qsTr("Import")
                        enabled: acceptable

                        onClicked: {
                            if (curPage === 1) {
                                curPage = 2
                            }
                            else {
                                if (rbPassword.checked) {
                                    noticeBox.passwordNotice = true
                                    noticeBox.password = confirmedPassworrdInput.text
                                    noticeBox.open()
                                }
                                // auth eID implementation will go here
                                else {
                                    encType = WalletInfo.Auth
                                    encKey = tiAuthId.text
                                }
                            }


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
                            abortBox.open()
                        }
                    }
                }
            }
        }
    }

    function toHex(str) {
        var hex = '';
        for (var i = 0; i < str.length; i++) {
            hex += '' + str.charCodeAt(i).toString(16);
        }
        return hex;
    }

    onAccepted: {
        if (rbPassword.checked) {
            password = confirmedPassworrdInput.text
            encType = WalletInfo.Password
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
