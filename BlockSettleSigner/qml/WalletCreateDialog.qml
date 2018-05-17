import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2
import com.blocksettle.PasswordConfirmValidator 1.0

CustomDialog {
    property bool primaryWalletExists: false
    property string walletName
    property string walletDesc
    property string password
    property bool isPrimary:    false
    property bool acceptable:   tfName.text.length && tfPassword.text.length && confirmPassword.acceptableInput
    property int inputLabelsWidth: 110

    id:root
    height: 340
    width: 400

    ColumnLayout {
        anchors.fill: parent
        Layout.fillHeight: true
        Layout.fillWidth: true
        spacing: 10
        width: parent.width

        RowLayout{
            CustomHeaderPanel{
                id: panelHeader
                Layout.preferredHeight: 40
                Layout.fillWidth: true
                text:  qsTr("Create New Wallet")
            }
        }

        RowLayout {
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomLabel {
                Layout.minimumWidth: inputLabelsWidth
                Layout.preferredWidth: inputLabelsWidth
                Layout.maximumWidth: inputLabelsWidth
                Layout.fillWidth: true
                text:   qsTr("Wallet Name:")
            }
            CustomTextInput {
                id: tfName
                Layout.fillWidth: true
                selectByMouse: true
                focus: true
            }
        }
        RowLayout {
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomLabel {
                Layout.minimumWidth: inputLabelsWidth
                Layout.preferredWidth: inputLabelsWidth
                Layout.maximumWidth: inputLabelsWidth
                Layout.fillWidth: true
                text:   qsTr("Wallet Description:")
            }
            CustomTextInput {
                id: tfDesc
                Layout.fillWidth: true
                selectByMouse: true
            }
        }

        RowLayout {
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomLabel {
                Layout.minimumWidth: inputLabelsWidth
                Layout.preferredWidth: inputLabelsWidth
                Layout.maximumWidth: inputLabelsWidth
                Layout.fillWidth: true
                text:   qsTr("Wallet Password:")
            }
            CustomTextInput {
                id: tfPassword
                Layout.fillWidth: true
                echoMode: TextField.Password
                selectByMouse: true
            }
        }

        RowLayout {
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomLabel {
                Layout.minimumWidth: inputLabelsWidth
                Layout.preferredWidth: inputLabelsWidth
                Layout.maximumWidth: inputLabelsWidth
                Layout.fillWidth: true
                text:   qsTr("Confirm Password:")
            }
            CustomTextInput {
                id: confirmPassword
                Layout.fillWidth: true
                echoMode: TextField.Password
                selectByMouse: true
                validator: PasswordConfirmValidator {
                    id: walletPasswordValidator
                    compareTo: tfPassword.text
                }
            }
        }

        RowLayout {
            visible: walletPasswordValidator.statusMsg !== ""
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomLabel {
                Layout.leftMargin: inputLabelsWidth + 5
                Layout.fillWidth: true
                text: walletPasswordValidator.statusMsg
                color: confirmPassword.acceptableInput ? "green" : "red"
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
                    enabled:    acceptable
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
    }
}
