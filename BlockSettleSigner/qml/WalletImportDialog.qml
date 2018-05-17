import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2
import QtQuick.Dialogs 1.2
import com.blocksettle.EasyEncValidator 1.0
import com.blocksettle.PasswordConfirmValidator 1.0

CustomDialog {
    property bool primaryWalletExists: false
    property bool digitalBackup: false
    property string walletName
    property string walletDesc
    property string password
    property string recoveryKey
    property bool isPrimary:    false
    property bool acceptable: tfName.text.length &&
                              tfPassword.text.length &&
                              confirmPassword.acceptableInput &&
                              (digitalBackup ? lblDBFile.text !== "..." :
                                               keyLine1.acceptableInput && keyLine2.acceptableInput) &&
                              (digitalBackup ? true :
                                               keyLine1.text !== keyLine2.text)

    property int inputLabelsWidth: 105
    property string paperBackupCode: keyLine1.text + " " + keyLine2.text
    width: 400
    height: !digitalBackup ? 440 : 370
    id:root



    ColumnLayout {
        anchors.fill: parent
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 10

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
                text:   qsTr("Wallet Password:")
            }
            CustomTextInput {
                id: tfPassword
                Layout.fillWidth: true
                selectByMouse: true
                echoMode: TextField.Password
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
                text:   qsTr("Confirm Password:")
            }
            CustomTextInput {
                id: confirmPassword
                Layout.fillWidth: true
                selectByMouse: true
                echoMode: TextField.Password
                validator: PasswordConfirmValidator { compareTo: tfPassword.text }
            }
        }

        RowLayout {
            visible: confirmPassword.validator.statusMsg !== ""
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomLabel {
                topPadding: 1
                bottomPadding: 1
                Layout.fillWidth: true
                Layout.leftMargin: inputLabelsWidth + 5
                text:  confirmPassword.validator.statusMsg
                color: confirmPassword.acceptableInput ? "green" : "red";
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


        RowLayout {
            visible: !digitalBackup
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomLabel {
                id: keyLine1Label
                Layout.fillWidth: true
                Layout.minimumWidth: inputLabelsWidth
                Layout.preferredWidth: inputLabelsWidth
                Layout.maximumWidth: inputLabelsWidth
                text: qsTr("Recovery key Line 1:")
            }

            CustomTextInput {
                id: keyLine1
                Layout.fillWidth: true
                selectByMouse: true
                activeFocusOnPress: true
                validator: EasyEncValidator { id: line1Validator; name: qsTr("Line 1") }
                onAcceptableInputChanged: {
                    if (acceptableInput && !keyLine2.acceptableInput) {
                        keyLine2.forceActiveFocus();
                    }
                }
            }
        }

        RowLayout {
            visible: !digitalBackup && line1Validator.statusMsg !== ""
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomLabel {
                topPadding: 1
                bottomPadding: 1
                Layout.fillWidth: true
                Layout.leftMargin: inputLabelsWidth + 5
                text:  line1Validator.statusMsg
                color: keyLine1.acceptableInput ? "green" : "red"
            }
        }

        RowLayout {
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            visible: !digitalBackup
            CustomLabel {
                id: keyLine2Label
                Layout.fillWidth: true
                Layout.minimumWidth: inputLabelsWidth
                Layout.preferredWidth: inputLabelsWidth
                Layout.maximumWidth: inputLabelsWidth
                text: qsTr("Recovery Key Line 2:")
            }

            CustomTextInput {
                id: keyLine2
                Layout.fillWidth: true
                validator: EasyEncValidator { id: line2Validator; name: qsTr("Line 2") }
                selectByMouse: true
                activeFocusOnPress: true
                onAcceptableInputChanged: {
                    if (acceptableInput && !keyLine1.acceptableInput) {
                        keyLine1.forceActiveFocus();
                    }
                }
            }
        }

        RowLayout {
            visible: !digitalBackup && line2Validator.statusMsg !== ""
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomLabel {
                topPadding: 1
                bottomPadding: 1
                Layout.fillWidth: true
                Layout.leftMargin: inputLabelsWidth + 5
                text:  keyLine1.text === keyLine2.text ?
                           qsTr("Same Code Used in Line 1 and Line 2") : line2Validator.statusMsg
                color: keyLine1.text === keyLine2.text || !keyLine2.acceptableInput ? "red" : "green"
            }
        }

        RowLayout {
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            visible: digitalBackup
            anchors.bottom: fillRect.top
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
            Layout.topMargin: 20
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
                        onClicked: root.close();
                    }
                }
            }
        }
    }

    onAccepted: {
        walletName = tfName.text
        walletDesc = tfDesc.text
        password = tfPassword.text
        isPrimary = cbPrimary.checked
        recoveryKey = digitalBackup ? lblDBFile.text : paperBackupCode
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
                filePath = filePath.replace(/(^file:\/{3})/, "")
                lblDBFile.text = decodeURIComponent(filePath)
            }
        }
    }
}
