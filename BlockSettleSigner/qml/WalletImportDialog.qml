import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2
import QtQuick.Dialogs 1.2


CustomDialog {
    property bool primaryWalletExists: false
    property bool digitalBackup: false
    property string walletName
    property string walletDesc
    property string password
    property string recoveryKey
    property bool isPrimary:    false
    property bool acceptable:   (tfName.text.length && tfPassword.text.length &&
                                 (digitalBackup ? (lblDBFile.text != "...")
                                                : walletsProxy.isValidPaperKey(taKey.text))
                                )

    property int inputLabelsWidth: 105
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
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomLabel {
                Layout.fillWidth: true
                Layout.minimumWidth: inputLabelsWidth
                Layout.preferredWidth: inputLabelsWidth
                Layout.maximumWidth: inputLabelsWidth
                text:   digitalBackup ? qsTr("Digital backup file:") : qsTr("Recovery key:")
            }
            CustomTextArea {
                visible: !digitalBackup
                id: taKey
                Layout.fillWidth: true
                placeholderText: qsTr("2 lines of 36-chars each encoded key or\nBech32-encoded or base58check-encoded key")
                selectByMouse: true

            }

            CustomLabel {
                visible:    digitalBackup
                id:     lblDBFile
                Layout.fillWidth: true
                Layout.maximumWidth: 120
                text:   "..."
                wrapMode: Label.Wrap
            }
            CustomButton {
                visible:    digitalBackup
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
        recoveryKey = digitalBackup ? lblDBFile.text : taKey.text
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
