import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2
import com.blocksettle.PasswordConfirmValidator 1.0


CustomDialog {
    property string walletName
    property string walletId
    property bool walletEncrypted
    property string oldPassword
    property string newPassword

    property bool acceptable: confirmPassword.acceptableInput
    property int inputLablesWidth: 110


    id: changeWalletPasswordDialog
    height: 270

    ColumnLayout {
        anchors.fill: parent
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 10

        RowLayout{
            Layout.alignment: Qt.AlignTop
            CustomHeaderPanel{
                id: panelHeader
                Layout.preferredHeight: 40
                Layout.fillWidth: true
                text:   qsTr("Change Password for Wallet %1").arg(walletName)

            }
        }

        RowLayout {
            spacing: 5
            Layout.alignment: Qt.AlignTop
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomLabel {
                visible:    walletEncrypted
                elide: Label.ElideRight
                text: qsTr("Current password:")
                wrapMode: Text.WordWrap
                Layout.minimumWidth: inputLablesWidth
                Layout.preferredWidth: inputLablesWidth
                Layout.maximumWidth: inputLablesWidth
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
            Layout.alignment: Qt.AlignTop

            CustomLabel {
                elide: Label.ElideRight
                text: qsTr("New Password:")
                wrapMode: Text.WordWrap
                Layout.minimumWidth: inputLablesWidth
                Layout.preferredWidth: inputLablesWidth
                Layout.maximumWidth: inputLablesWidth
                Layout.fillWidth: true
            }
            CustomTextInput {
                id: tfNewPassword1
                focus: true
                placeholderText: qsTr("New password")
                echoMode: TextField.Password
                Layout.fillWidth: true
            }
        }

        RowLayout {
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            Layout.alignment: Qt.AlignTop

            CustomLabel {
                elide: Label.ElideRight
                text: qsTr("Confirm New:")
                wrapMode: Text.WordWrap
                Layout.minimumWidth: inputLablesWidth
                Layout.preferredWidth: inputLablesWidth
                Layout.maximumWidth: inputLablesWidth
                Layout.fillWidth: true
            }
            CustomTextInput {
                id: confirmPassword
                focus: true
                placeholderText: qsTr("New password again")
                echoMode: TextField.Password
                Layout.fillWidth: true
                validator: PasswordConfirmValidator {
                    compareTo: tfNewPassword1.text
                }
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignTop
            opacity: confirmPassword.validator.statusMsg === "" ? 0.0 : 1.0
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomLabel {
                topPadding: 1
                bottomPadding: 1
                Layout.fillWidth: true
                Layout.leftMargin: inputLablesWidth + 5
                text:  confirmPassword.validator.statusMsg
                color: confirmPassword.acceptableInput ? "green" : "red";
            }
        }


        CustomButtonBar {
            Layout.alignment: Qt.AlignBottom
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
                        onClicked: changeWalletPasswordDialog.close();
                    }
                }

            }
        }
    }

    onAccepted: {
        oldPassword = tfOldPassword.text
        newPassword = confirmPassword.text
    }

}
