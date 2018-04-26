import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2

CustomDialog {
    property bool primaryWalletExists: false
    property string walletName
    property string walletDesc
    property string password
    property bool isPrimary:    false
    property bool acceptable:   (tfName.text.length && tfPassword.text.length)

    id:root

    ColumnLayout {
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
                Layout.minimumWidth: 110
                Layout.preferredWidth: 110
                Layout.maximumWidth: 110
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
                Layout.minimumWidth: 110
                Layout.preferredWidth: 110
                Layout.maximumWidth: 110
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
                Layout.minimumWidth: 110
                Layout.preferredWidth: 110
                Layout.maximumWidth: 110
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

            CustomCheckBox {
                id: cbPrimary
                Layout.fillWidth: true
                Layout.leftMargin: 110 + 5
                enabled: !primaryWalletExists
                text:   qsTr("Primary Wallet")
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
