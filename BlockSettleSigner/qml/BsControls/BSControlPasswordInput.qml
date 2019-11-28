import QtQuick 2.9
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.11

import "../StyledControls"
import "../BsStyles"

import com.blocksettle.PasswordDialogData 1.0
import com.blocksettle.QPasswordData 1.0

CustomTitleDialogWindow {
    id: root

    enum ControlPasswordStatus
    {
       Accepted,
       Rejected,
       RequestedNew
    }

    property QPasswordData passwordData: QPasswordData{}
    property int controlPasswordStatus

    property string decryptHeaderText: qsTr("Enter Control Password")

    title: controlPasswordStatus === BSControlPasswordInput.ControlPasswordStatus.RequestedNew
               ? qsTr("Encrypt Wallets Storage")
               : qsTr("Decrypt Wallets Storage")

    width: 350
    rejectable: false

    cContentItem: ColumnLayout {
        id: contentItemData
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignTop
        Layout.margins: 0

        ColumnLayout {
            Layout.preferredWidth: root.width
            spacing: 0
            Layout.margins: 0
            Layout.alignment: Qt.AlignTop


            CustomHeader {
                id: decryptHeader
                Layout.alignment: Qt.AlignTop
                text: decryptHeaderText
                Layout.fillWidth: true
                Layout.preferredHeight: 25
                Layout.topMargin: 5
                Layout.leftMargin: 10
                Layout.rightMargin: 10
            }

            RowLayout {
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                Layout.topMargin: 5
                Layout.bottomMargin: 5

                CustomLabel {
                    visible: controlPasswordStatus === BSControlPasswordInput.ControlPasswordStatus.Rejected
                    Layout.fillWidth: true
                    Layout.minimumWidth: 110
                    Layout.preferredWidth: 110
                    Layout.maximumWidth: 110
                    text: qsTr("Password")
                }

                CustomTextInput {
                    id: passwordInputDecrypt
                    visible: controlPasswordStatus === BSControlPasswordInput.ControlPasswordStatus.Rejected
                    Layout.fillWidth: true
                    Layout.topMargin: 5
                    Layout.bottomMargin: 5
                    focus: true
                    echoMode: TextField.Password
                    //placeholderText: qsTr("Password")

                    Keys.onEnterPressed: {
                        if (btnAccept.enabled) btnAccept.onClicked()
                    }
                    Keys.onReturnPressed: {
                        if (btnAccept.enabled) btnAccept.onClicked()
                    }
                }
            }

            BSConfirmedPasswordInput {
                id: newPasswordWithConfirm
                visible: controlPasswordStatus === BSControlPasswordInput.ControlPasswordStatus.RequestedNew
                columnSpacing: 10
                passwordLabelTxt: qsTr("Control Password")
                confirmLabelTxt: qsTr("Confirm Password")
                onConfirmInputEnterPressed: {
                    if (btnAccept.enabled) btnAccept.onClicked()
                }
            }
        }
    }

    cFooterItem: RowLayout {
        Layout.fillWidth: true
        CustomButtonBar {
            id: barFooter
            Layout.fillWidth: true

            CustomButton {
                id: btnReject
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.margins: 5
                text: qsTr("Cancel")
                onClicked: {
                    rejectAnimated()
                }
            }

            CustomButton {
                id: btnAccept
                enabled: controlPasswordStatus === BSControlPasswordInput.ControlPasswordStatus.RequestedNew
                         ? newPasswordWithConfirm.acceptableInput
                         : passwordInputDecrypt.text.length >= 6
                primary: true
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 5
                text: qsTr("Ok")
                onClicked: {
                    if (controlPasswordStatus === BSControlPasswordInput.ControlPasswordStatus.Rejected) {
                        passwordData.textPassword = passwordInputDecrypt.text
                    }
                    else if (controlPasswordStatus === BSControlPasswordInput.ControlPasswordStatus.RequestedNew) {
                        passwordData.textPassword = newPasswordWithConfirm.password
                    }
                    passwordData.encType = QPasswordData.Password
                    acceptAnimated()
                }
            }
        }
    }
}

