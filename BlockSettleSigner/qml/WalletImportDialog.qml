import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2
import QtQuick.Dialogs 1.2
import com.blocksettle.PasswordConfirmValidator 1.0
import com.blocksettle.WalletSeed 1.0
import com.blocksettle.WalletInfo 1.0
import com.blocksettle.FrejaProxy 1.0
import com.blocksettle.FrejaSignWalletObject 1.0

import "bscontrols"

CustomDialog {
    property bool primaryWalletExists: false
    property bool digitalBackup: false
    property string password
    property bool isPrimary:    false
    property WalletSeed seed
    property FrejaSignWalletObject  frejaSign
    property bool acceptable: tfName.text.length &&
                              (confirmedPassworrdInput.acceptableInput || password.length) &&
                              (digitalBackup ? lblDBFile.text !== "..." : rootKeyInput.acceptableInput)
    property int inputLabelsWidth: 105

    implicitWidth: 400
    implicitHeight: mainLayout.implicitHeight
    id: root

    Connections {
        target: seed
        onError: {
            ibFailure.displayMessage(errMsg)
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
                root.close();
                event.accepted = true;
            }
        }

        ColumnLayout {
            anchors.fill: parent
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10
            id: mainLayout

            RowLayout{
                Layout.fillWidth: true
                CustomHeaderPanel{
                    id: panelHeader
                    Layout.preferredHeight: 40
                    Layout.fillWidth: true
                    text:   qsTr("Import Wallet")
                }
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
                    Layout.fillWidth: true
                    Layout.minimumWidth: inputLabelsWidth
                    Layout.preferredWidth: inputLabelsWidth
                    Layout.maximumWidth: inputLabelsWidth
                    text:   qsTr("Wallet Name:")
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
                visible: !digitalBackup
                spacing: 5
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10

                CustomLabel {
                    Layout.fillWidth: true
                    Layout.minimumWidth: inputLabelsWidth
                    Layout.preferredWidth: inputLabelsWidth
                    Layout.maximumWidth: inputLabelsWidth
                    text:   qsTr("Wallet Description:")
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
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10

                CustomRadioButton {
                    id: rbPassword
                    text:   qsTr("Password")
                    checked:    true
                }
                CustomRadioButton {
                    id: rbFreja
                    text:   qsTr("Freja eID")
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
                visible:    rbFreja.checked
                spacing: 5
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10

                CustomTextInput {
                    id: tiFrejaId
                    placeholderText: qsTr("Freja ID (email)")
                }
                CustomButton {
                    id: btnFreja
                    text:   !frejaSign ? qsTr("Sign with Freja") : frejaSign.status
                    enabled:    !frejaSign && tiFrejaId.text.length
                    onClicked: {
                        seed.encType = WalletInfo.Freja
                        seed.encKey = tiFrejaId.text
                        password = ''
                        frejaSign = freja.signWallet(tiFrejaId.text, qsTr("Password for wallet %1").arg(tfName.text),
                                                              seed.walletId)
                        btnFreja.enabled = false
                        frejaSign.success.connect(function(key) {
                            password = key
                            text = qsTr("Successfully signed")
                        })
                        frejaSign.error.connect(function(text) {
                            frejaSign = null
                            btnFreja.enabled = tiFrejaId.text.length
                        })
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

            BSEasyCodeInput {
                id: rootKeyInput
                visible: !digitalBackup
                rowSpacing: 0
                columnSpacing: 0
                Layout.topMargin: 5
                sectionHeaderTxt: qsTr("Enter Root Private Key: ")
                line1LabelTxt: qsTr("Root Key Line 1")
                line2LabelTxt: qsTr("Root Key Line 2")
                onEntryComplete: {
                    if (!seed.parsePaperKey(privateRootKey)) {
                        ibFailure.displayMessage("Failed to parse paper backup key")
                    }
                }
            }

            RowLayout {
                spacing: 5
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                visible: digitalBackup

                CustomLabel {
                    Layout.fillWidth: true
                    Layout.minimumWidth: inputLabelsWidth
                    Layout.preferredWidth: inputLabelsWidth
                    Layout.maximumWidth: inputLabelsWidth
                    text: qsTr("Digital backup:")
                }

                CustomLabel {
                    id:     lblDBFile
                    Layout.fillWidth: true
                    Layout.maximumWidth: 120
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

            Rectangle {
                id: fillRect
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
                        text:   qsTr("CONFIRM")
                        enabled: acceptable

                        onClicked: {
                            accept();
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
        for (var i = 0; i < str.length; i++) {
            hex += '' + str.charCodeAt(i).toString(16);
        }
        return hex;
    }

    onAccepted: {
        if (rbPassword.checked) {
            password = toHex(confirmedPassworrdInput.text)
        }
        isPrimary = cbPrimary.checked
    }

    onRejected: {
        frejaSign.cancel()
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
                filePath = filePath.replace(/(^file:\/{2})/, "")
                lblDBFile.text = decodeURIComponent(filePath)
                if (!seed.parseDigitalBackupFile(lblDBFile.text)) {
                    ibFailure.displayMessage(qsTr("Failed to parse digital backup from %1").arg(lblDBFile.text))
                }
            }
        }
    }
}
