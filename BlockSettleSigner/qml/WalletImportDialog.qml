import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2
import QtQuick.Dialogs 1.2
import com.blocksettle.EasyEncValidator 1.0

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
                              (digitalBackup ? lblDBFile.text != "..." : keyLine1.acceptableInput && keyLine2.acceptableInput) &&
                              (digitalBackup ? true : keyLine1.acceptableInput != keyLine2.acceptableInput)

    property int inputLabelsWidth: 105
    property string paperBackupCode: keyLine1.text + " " + keyLine2.text
    width: 400
    height: 350
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
                Layout.fillWidth: true
                Layout.minimumWidth: inputLabelsWidth
                Layout.preferredWidth: inputLabelsWidth
                Layout.maximumWidth: inputLabelsWidth
                text: qsTr("Recovery key line 1:")
            }

            CustomTextInput {
                id: keyLine1
                Layout.fillWidth: true
                selectByMouse: true
                activeFocusOnPress: true
                validator: EasyEncValidator {}
                onAcceptableInputChanged: {
                    if (acceptableInput && !keyLine2.acceptableInput) {
                        console.log("LINE ONE ACCEPTED")
                        keyLine2.forceActiveFocus();
                    }
                }
            }
        }

        RowLayout {
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            visible: !digitalBackup
            CustomLabel {
                Layout.fillWidth: true
                Layout.minimumWidth: inputLabelsWidth
                Layout.preferredWidth: inputLabelsWidth
                Layout.maximumWidth: inputLabelsWidth
                text: qsTr("Recovery key line 2:")
            }

            CustomTextInput {
                id: keyLine2
                Layout.fillWidth: true
                validator: EasyEncValidator {}
                selectByMouse: true
                activeFocusOnPress: true
                onAcceptableInputChanged: {
                    if (acceptableInput && !keyLine1.acceptableInput) {
                        console.log("LINE TWO ACCEPTED")
                        keyLine1.forceActiveFocus();
                    }
                }

            }
        }
        RowLayout {
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            visible: !digitalBackup

            CustomLabel {
                Layout.fillWidth: true
                Layout.minimumWidth: inputLabelsWidth
                Layout.preferredWidth: inputLabelsWidth
                Layout.maximumWidth: inputLabelsWidth
                text: qsTr("")
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
                        ldrDBFileDlg.active = true
                    }
                    ldrDBFileDlg.item.open();
                }
            }
        }

        Rectangle {
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
