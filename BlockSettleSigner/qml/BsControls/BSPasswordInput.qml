import QtQuick 2.9
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.11

import "../StyledControls"
import "../BsStyles"

// use cases
// - check password by setting property 'passwordToCheck' to non-empty value
// - just enter password and get it from 'enteredPassword' property

BSMessageBox {
    id: root

    property string passwordToCheck
    property alias enteredPassword : passwordInput.text
    property bool passwordCorrect: passwordInput.text.length !== 0 && (passwordToCheck.length ===0 || passwordInput.text === passwordToCheck)

    acceptButton.enabled: passwordCorrect

    title: qsTr("Notice!")
    customText: qsTr("Please take care of your assets!")
    customDetails: qsTr("No one can help you recover your bitcoins if you forget the passphrase and don't have a backup! Your Wallet and any backups are useless if you lose them.<br><br>A backup protects your wallet forever, against hard drive loss and losing your passphrase. It also protects you from theft, if the wallet was encrypted and the backup wasn't stolen with it. Please make a backup and keep it in a safe place.<br><br>Please enter your passphrase one more time to indicate that you are aware of the risks of losing your passphrase!")

    labelText.color: BSStyle.dialogTitleGreenColor

    onVisibleChanged: {
        passwordInput.text = ""
    }

    messageDialogContentItem: RowLayout {
        CustomLabel {
            Layout.fillWidth: true
            Layout.topMargin: 5
            Layout.bottomMargin: 5
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            Layout.preferredWidth: 105
            text:   qsTr("Password")
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
        }
    }
}

