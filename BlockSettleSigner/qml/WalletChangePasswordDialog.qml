import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2

CustomDialog {
    property string walletName
    property string walletId
    property bool walletEncrypted
    property string oldPassword
    property string newPassword

    id: changeWalletPasswordDialog


    function isAcceptable() {
        if (walletEncrypted && !tfOldPassword.text.length) {
            return false
        }
        if (!tfNewPassword1.text.length || !tfNewPassword2.length || (tfNewPassword1.text != tfNewPassword2.text)) {
            return false
        }
        return true
    }

    ColumnLayout {

        Layout.fillWidth: true
        spacing: 10

        RowLayout{
            CustomHeaderPanel{
                id: panelHeader
                Layout.preferredHeight: 40
                Layout.fillWidth: true
                text:   qsTr("Backup Private Key for Wallet %1").arg(walletName)

            }
        }

        RowLayout {
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomLabel {
                visible:    walletEncrypted
                elide: Label.ElideRight
                text: qsTr("Current password:")
                wrapMode: Text.WordWrap
                Layout.minimumWidth: 110
                Layout.preferredWidth: 110
                Layout.maximumWidth: 110
                Layout.fillWidth: true
            }
            CustomTextInput {
                id: tfOldPassword
                visible: walletEncrypted
                focus: true
                placeholderText: qsTr("Old Password")
                echoMode: TextField.Password
                Layout.fillWidth: true
            }
        }

        RowLayout {
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomLabel {
                elide: Label.ElideRight
                text: qsTr("New Password 1:")
                wrapMode: Text.WordWrap
                Layout.minimumWidth: 110
                Layout.preferredWidth: 110
                Layout.maximumWidth: 110
                Layout.fillWidth: true
            }
            CustomTextInput {
                id: tfNewPassword1
                focus: true
                placeholderText: qsTr("New password")
                echoMode: TextField.Password
                Layout.fillWidth: true

                onAccepted: {
                    if (isAcceptable()) {
                        accept()
                    }
                }
            }
        }

        RowLayout {
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomLabel {
                elide: Label.ElideRight
                text: qsTr("New Password 2:")
                wrapMode: Text.WordWrap
                Layout.minimumWidth: 110
                Layout.preferredWidth: 110
                Layout.maximumWidth: 110
                Layout.fillWidth: true
            }
            CustomTextInput {
                id: tfNewPassword2
                focus: true
                placeholderText: qsTr("New password again")
                echoMode: TextField.Password
                Layout.fillWidth: true

                onAccepted: {
                    if (isAcceptable()) {
                        accept()
                    }
                }
            }
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
                        onClicked: changeWalletPasswordDialog.close();
                    }
                }

            }
        }
    }

    onAccepted: {
        oldPassword = tfOldPassword.text
        newPassword = tfNewPassword2.text
    }

}
