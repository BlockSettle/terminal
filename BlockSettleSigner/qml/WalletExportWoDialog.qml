import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2

CustomDialog {
    property string walletName
    property string walletId
    property bool walletEncrypted
    property string woWalletFileName
    property string password
    property bool acceptable: !walletEncrypted
    property string exportDir:  Qt.resolvedUrl(".")

    id: exportWoWalletDialog
    width: 400

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10
        width: parent.width

        RowLayout{
            CustomHeaderPanel{
                Layout.preferredHeight: 40
                id: panelHeader
                Layout.fillWidth: true
                text:  qsTr("Export Watching-Only Copy of %1").arg(walletName)

            }
        }

        RowLayout {
            width: parent.width
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomLabel {
                visible:    walletEncrypted
                elide: Label.ElideRight
                text: qsTr("Password:")
                Layout.minimumWidth: 110
                Layout.preferredWidth: 110
                Layout.maximumWidth: 110
                Layout.fillWidth: true
            }
            CustomTextInput {
                id: tfPassword
                visible: walletEncrypted
                focus: true
                placeholderText: qsTr("Wallet password")
                echoMode: TextField.Password
                Layout.fillWidth: true
                onTextChanged: {
                    acceptable = (text.length > 0)
                }

                onAccepted: {
                    if (text && text.length > 0) {
                        accept()
                    }
                }
            }
        }

        RowLayout {
            width: parent.width
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomLabel {
                Layout.minimumWidth: 110
                Layout.preferredWidth: 110
                Layout.maximumWidth: 110
                text:   qsTr("Export to file:")
                Layout.fillWidth: true
                Layout.fillHeight: true
                verticalAlignment: Text.AlignTop
            }
            CustomLabelValue {
                text:   qsTr("%1/%2").arg(exportDir).arg(woWalletFileName)
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }

        RowLayout {
            width: parent.width
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomButton {
                text:   qsTr("Select Target Dir")
                Layout.minimumWidth: 80
                Layout.preferredWidth: 80
                Layout.maximumWidth: 80
                Layout.maximumHeight: 25
                Layout.leftMargin: 110 + 5
                onClicked: {
                    if (!ldrWoWalletDirDlg.item) {
                        ldrWoWalletDirDlg.active = true
                    }
                    ldrWoWalletDirDlg.startFromFolder = exportDir
                    ldrWoWalletDirDlg.item.accepted.connect(function() {
                        exportDir = ldrWoWalletDirDlg.dir
                    })
                    ldrWoWalletDirDlg.item.open();
                }
            }
        }

        CustomButtonBar {
            implicitHeight: childrenRect.height
            implicitWidth: changeWalletPasswordDialog.width
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
                    enabled: acceptable
                    text:   qsTr("CONFIRM")
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
                        onClicked: exportWoWalletDialog.close();
                    }
                }

            }
        }
    }

    onAccepted: {
        password = tfPassword.text
    }
}
