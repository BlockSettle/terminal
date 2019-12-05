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
    property QPasswordData passwordDataOld: QPasswordData{}
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
            CustomLabel{
                id: labelDetails_
                visible: controlPasswordStatus === BSControlPasswordInput.ControlPasswordStatus.RequestedNew
                text: qsTr("Your wallet files contain metadata such as your public keys (addresses), chatID and chat history. \
With Public Data Encryption enabled you will be required to decrypt this material on each Terminal launch. \
<br><br>THIS PASSWORD WILL BE USED FOR ALL WALLETS")
                padding: 10
                textFormat: Text.RichText
                Layout.preferredWidth: root.width - 20
                horizontalAlignment: Text.AlignLeft
                Layout.leftMargin: 10
                Layout.rightMargin: 10

                onLinkActivated: Qt.openUrlExternally(link)
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                }
            }
            RowLayout {
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                Layout.topMargin: 5
                Layout.bottomMargin: 5

                CustomLabel {
                    visible: controlPasswordStatus !== BSControlPasswordInput.ControlPasswordStatus.RequestedNew
                    Layout.fillWidth: true
                    Layout.minimumWidth: 110
                    Layout.preferredWidth: 110
                    Layout.maximumWidth: 110
                    text: controlPasswordStatus === BSControlPasswordInput.ControlPasswordStatus.Rejected
                          ? qsTr("Password")
                          : qsTr("Old Password")
                }

                CustomTextInput {
                    id: passwordInputDecrypt
                    visible: controlPasswordStatus !== BSControlPasswordInput.ControlPasswordStatus.RequestedNew
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
                visible: controlPasswordStatus !== BSControlPasswordInput.ControlPasswordStatus.Rejected
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
                text: controlPasswordStatus !== BSControlPasswordInput.ControlPasswordStatus.Rejected
                    ? qsTr("Skip")
                    : qsTr("Cancel")
                onClicked: {
                    rejectAnimated()
                }
            }

            CustomButton {
                id: btnAccept
                enabled: {
                    if (controlPasswordStatus === BSControlPasswordInput.ControlPasswordStatus.RequestedNew)
                        return newPasswordWithConfirm.acceptableInput
                    else if (controlPasswordStatus === BSControlPasswordInput.ControlPasswordStatus.Rejected)
                        return passwordInputDecrypt.text.length >= 6
                    else
                        return newPasswordWithConfirm.acceptableInput && passwordInputDecrypt.text.length >= 6
                }

                primary: true
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 5
                text: qsTr("Ok")
                onClicked: {
                    passwordDataOld.textPassword = passwordInputDecrypt.text

                    if (controlPasswordStatus === BSControlPasswordInput.ControlPasswordStatus.Rejected) {
                        passwordData.textPassword = passwordInputDecrypt.text
                    }
                    else {
                        passwordData.textPassword = newPasswordWithConfirm.password
                    }

                    passwordData.encType = QPasswordData.Password
                    acceptAnimated()
                }
            }
        }
    }
}

