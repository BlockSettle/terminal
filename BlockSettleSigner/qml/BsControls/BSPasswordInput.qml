import QtQuick 2.9
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.11

import "../StyledControls"
import "../BsStyles"

CustomTitleDialogWindow {
    id: root

    property alias enteredPassword : passwordInput.text
    default property alias details: detailsContainer.data

    property alias btnReject : btnReject
    property alias btnAccept : btnAccept
    property alias passwordInput : passwordInput

    title: qsTr("Decrypt Wallet")
    width: 350
    rejectable: true

//    Component.onCompleted: {
//        details.parent = detailsContainer
//    }

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


            ColumnLayout {
                id: detailsContainer
                Layout.margins: 0
                spacing: 0
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
            }

            CustomHeader {
                Layout.alignment: Qt.AlignTop
                text: qsTr("Enter password")
                Layout.fillWidth: true
                Layout.preferredHeight: 25
                Layout.topMargin: 5
                Layout.leftMargin: 10
                Layout.rightMargin: 10
            }

            RowLayout {
                CustomLabel {
                    Layout.fillWidth: true
                    Layout.topMargin: 5
                    Layout.bottomMargin: 5
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                    Layout.preferredWidth: 110
                    text: qsTr("Password")
                }
                CustomTextInput {
                    id: passwordInput
                    Layout.topMargin: 5
                    Layout.bottomMargin: 5
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                    focus: true
                    echoMode: TextField.Password
                    //placeholderText: qsTr("Password")
                    Layout.fillWidth: true

                    Keys.onEnterPressed: {
                        if (btnAccept.enabled) btnAccept.onClicked()
                    }
                    Keys.onReturnPressed: {
                        if (btnAccept.enabled) btnAccept.onClicked()
                    }
                }
            }
        }
    }

    cFooterItem: RowLayout {
        Layout.fillWidth: true
        CustomButtonBar {
            id: buttonBar
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

            CustomButtonPrimary {
                id: btnAccept
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 5
                text: qsTr("Ok")
                onClicked: {
                    acceptAnimated()
                }
            }
        }
    }
}

