import QtQuick 2.9
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.11

import "../StyledControls"
import "../BsStyles"

// used for password confirmation in wallet create/import dialog
CustomTitleDialogWindow {
    id: root

    property string passwordToCheck
    property alias enteredPassword : passwordInput.text
    property bool passwordCorrect: passwordInput.text.length !== 0 && (passwordToCheck.length ===0 || passwordInput.text === passwordToCheck)

    title: type === BSPasswordInput.Type.Request ? qsTr("Decrypt Wallet") : qsTr("Notice!")
    width: 350
    height: 70 + headerPanelHeight + contentItemData.height

    rejectable: true

    enum Type {
       Confirm = 0,
       Request = 1
    }
    property int type: BSPasswordInput.Type.Confirm

    cContentItem: ColumnLayout {
        Layout.fillWidth: true
        ColumnLayout {
            Layout.preferredWidth: root.width

            CustomHeader {
                text: qsTr("Please take care of your assets!")
                visible: type === BSPasswordInput.Type.Confirm
                Layout.fillWidth: true
                Layout.preferredHeight: 25
                Layout.topMargin: 5
                Layout.leftMargin: 10
                Layout.rightMargin: 10
            }

            CustomLabel{
                id: labelDetails_
                visible: type === BSPasswordInput.Type.Confirm
                text: qsTr("No one can help you recover your bitcoins if you forget the passphrase and don't have a backup! \
Your Wallet and any backups are useless if you lose them.\
<br><br>A backup protects your wallet forever, against hard drive loss and losing your passphrase. \
It also protects you from theft, if the wallet was encrypted and the backup wasn't stolen with it. \
Please make a backup and keep it in a safe place.\
<br><br>Please enter your passphrase one more time to indicate that you are aware of the risks of losing your passphrase!")
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

            CustomHeader {
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
            Layout.preferredHeight: 45

            CustomButton {
                id: rejectButton_
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
                enabled: passwordCorrect
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 5
                text: type === BSPasswordInput.Type.Request ? qsTr("Ok") : qsTr("Continue")
                onClicked: {
                    acceptAnimated()
                }
            }
        }
    }
}

