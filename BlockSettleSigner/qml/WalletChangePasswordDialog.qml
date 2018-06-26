import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2
import com.blocksettle.PasswordConfirmValidator 1.0

import "bscontrols"


CustomDialog {
    id: changeWalletPasswordDialog

    property string walletName
    property string walletId
    property bool walletEncrypted
    property string oldPassword
    property string newPassword
    property bool acceptable: newPasswordWithConfirm.acceptableInput &&
                              tfOldPassword.text.length
    property int inputLablesWidth: 110

    implicitWidth: 400
    implicitHeight: mainLayout.childrenRect.height

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
                changeWalletPasswordDialog.close();
                event.accepted = true;
            }
        }

        ColumnLayout {

            id: mainLayout
            Layout.fillWidth: true
            spacing: 10

            RowLayout{
                CustomHeaderPanel{
                    id: panelHeader
                    Layout.preferredHeight: 40
                    Layout.fillWidth: true
                    text:   qsTr("Change Password for Wallet %1").arg(walletName)

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

            BSConfirmedPasswordInput {
                id: newPasswordWithConfirm
                columnSpacing: 10
                passwordLabelTxt: qsTr("New Password")
                passwordInputPlaceholder: qsTr("New Password")
                confirmLabelTxt: qsTr("Confirm New")
                confirmInputPlaceholder: qsTr("Confirm New Password")
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
                            onClicked: changeWalletPasswordDialog.close();
                        }
                    }

                }
            }
        }
    }

    onAccepted: {
        oldPassword = tfOldPassword.text
        newPassword = newPasswordWithConfirm.text
    }

}
