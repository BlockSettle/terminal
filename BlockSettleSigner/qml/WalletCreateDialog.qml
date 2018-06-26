import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2
import com.blocksettle.PasswordConfirmValidator 1.0

import "bscontrols"

CustomDialog {
    property bool primaryWalletExists: false
    property string walletName
    property string walletDesc
    property string password
    property bool isPrimary:    false
    property bool acceptable:   tfName.text.length &&
                                newPasswordWithConfirm.acceptableInput
    property int inputLabelsWidth: 110

    id:root
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
                root.close();
                event.accepted = true;
            }
        }

        ColumnLayout {
            anchors.fill: parent
            Layout.fillHeight: true
            Layout.fillWidth: true
            spacing: 10
            width: parent.width
            id: mainLayout

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

            BSConfirmedPasswordInput {
                id: newPasswordWithConfirm
                columnSpacing: 10
                passwordLabelTxt: qsTr("Wallet Password")
                passwordInputPlaceholder: qsTr("New Wallet Password")
                confirmLabelTxt: qsTr("Confirm Password")
                confirmInputPlaceholder: qsTr("Confirm New Wallet Password")
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
                implicitHeight: childrenRect.height
                implicitWidth: root.width
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
    }

    onAccepted: {
        walletName = tfName.text
        walletDesc = tfDesc.text
        password = newPasswordWithConfirm.text
        isPrimary = cbPrimary.checked
    }
}
