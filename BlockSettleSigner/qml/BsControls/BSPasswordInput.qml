import QtQuick 2.9
import QtQuick.Controls 2.4
import QtQuick.Controls.Styles 1.4
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



//CustomDialog {
//    id: root
//    width: 350

//    property alias acceptButtonText: acceptButton.text
//    property alias rejectButtonText: rejectButton.text
//    property string passwordToCheck
//    property alias enteredPassword : passwordInput.text
//    property bool acceptable: passwordInput.text.length !== 0 && (passwordToCheck.length ===0 || passwordInput.text === passwordToCheck)
//    property alias textFormat: labelDetails.textFormat

//    property string customTitle: qsTr("Notice!")
//    property string customText: qsTr("Please take care of your assets!")
//    property string customDetails: qsTr("No one can help you recover your bitcoins if you forget the passphrase and don't have a backup! Your Wallet and any backups are useless if you lose them.<br><br>A backup protects your wallet forever, against hard drive loss and losing your passphrase. It also protects you from theft, if the wallet was encrypted and the backup wasn't stolen with it. Please make a backup and keep it in a safe place.<br><br>Please enter your passphrase one more time to indicate that you are aware of the risks of losing your passphrase!")

//    onOpened: {
//        passwordInput.text = ""
//    }

//    contentItem: FocusScope {
//        id: focus
//        anchors.fill: parent
//        anchors.margins: 0
//        focus: true

//        Keys.onPressed: {
//            if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
//                if (acceptable) {
//                    acceptAnimated()
//                }
//            } else if (event.key === Qt.Key_Escape) {
//                reject()
//            }
//        }
//        ColumnLayout {
//            anchors.fill: parent
//            anchors.margins: 0

//            CustomHeaderPanel {
//                id: labelTitle
//                Layout.preferredHeight: 40
//                Layout.fillWidth: true
//                text: customTitle
//            }
//            RowLayout {
//                id: rowLayout_0
//                Layout.fillWidth: true
//                spacing: 5
//                Layout.preferredWidth: root.width

//                Image {
//                    id: image
//                    Layout.margins: 10
//                    Layout.rightMargin: 0
//                    Layout.alignment: Qt.AlignTop
//                    source: "/NOTIFICATION_INFO"
//                }

//                ColumnLayout {
//                    Layout.preferredWidth: root.width

//                    CustomLabelValue{
//                        id: labelText
//                        padding: 15
//                        Layout.preferredWidth: root.width - image.width - 10
//                        color: BSStyle.dialogTitleGreenColor
//                        Layout.minimumHeight: 40
//                        text: customText
//                    }

//                    CustomLabel {
//                        id: labelDetails
//                        text: customDetails
//                        padding: 15
//                        textFormat: Text.RichText
//                        Layout.preferredWidth: root.width - image.width - 10
//                        onLinkActivated: Qt.openUrlExternally(link)
//                        MouseArea {
//                            anchors.fill: parent
//                            acceptedButtons: Qt.NoButton // we don't want to eat clicks on the Text
//                            cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
//                        }
//                    }
//                }
//            }

//            RowLayout {
//                CustomLabel {
//                    Layout.fillWidth: true
//                    Layout.topMargin: 5
//                    Layout.bottomMargin: 5
//                    Layout.leftMargin: 10
//                    Layout.rightMargin: 10
//                    Layout.preferredWidth: 105
//                    text:   qsTr("Password")
//                }
//                CustomTextInput {
//                    id: passwordInput
//                    Layout.topMargin: 5
//                    Layout.bottomMargin: 5
//                    Layout.leftMargin: 10
//                    Layout.rightMargin: 10
//                    focus: true
//                    echoMode: TextField.Password
//                    //placeholderText: qsTr("Password")
//                    Layout.fillWidth: true
//                }
//            }

//            CustomButtonBar {
//                id: buttonBar
//                Layout.preferredHeight: childrenRect.height + 10
//                Layout.fillWidth: true

//                CustomButton {
//                    id: rejectButton
//                    anchors.left: parent.left
//                    anchors.bottom: parent.bottom
//                    text: qsTr("Cancel")
//                    onClicked: {
//                        reject()
//                    }
//                }
//                CustomButtonPrimary {
//                    id: acceptButton
//                    anchors.right: parent.right
//                    anchors.bottom: parent.bottom
//                    text: qsTr("OK")
//                    enabled: acceptable
//                    onClicked: {
//                        acceptAnimated()
//                    }
//                }
//            }
//        }
//    }
//}
