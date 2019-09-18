import QtQuick 2.9
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.11

import "../StyledControls"
import "../BsStyles"

import com.blocksettle.PasswordDialogData 1.0
import com.blocksettle.AutheIDClient 1.0
import com.blocksettle.AuthSignWalletObject 1.0
import com.blocksettle.WalletInfo 1.0
import com.blocksettle.QPasswordData 1.0

CustomTitleDialogWindow {
    id: root

    property AuthSignWalletObject  authSign: AuthSignWalletObject{}
    property QPasswordData passwordData: QPasswordData{}

    property alias enteredPassword : passwordInput.text
    default property alias details: detailsContainer.data

    property alias btnReject : btnReject
    property alias btnAccept : btnAccept
    property alias passwordInput : passwordInput

    readonly property int duration: 30

    title: qsTr("Decrypt Wallet")
    width: 350
    rejectable: true

    onBsRejected: {
        if (authSign) {
            authSign.cancel()
        }
    }

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
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                Layout.topMargin: 5
                Layout.bottomMargin: 5

                CustomLabel {
                    visible: walletInfo.encType === QPasswordData.Password
                    Layout.fillWidth: true
                    Layout.minimumWidth: 110
                    Layout.preferredWidth: 110
                    Layout.maximumWidth: 110
                    text: qsTr("Password")
                }

                CustomTextInput {
                    id: passwordInput
                    visible: walletInfo.encType === QPasswordData.Password
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

                CustomLabel {
                    id: labelAuth
                    visible: walletInfo.encType === QPasswordData.Auth
                    text: authSign.status
                }
            }

            ColumnLayout {
                visible: walletInfo.encType === QPasswordData.Auth
                spacing: 5
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10

                Timer {
                    id: timer
                    property real timeLeft: duration
                    interval: 500
                    running: true
                    repeat: true
                    onTriggered: {
                        timeLeft -= 0.5
                        if (timeLeft <= 0) {
                            stop()
                            rejectAnimated()
                        }
                    }
                    signal expired()
                }

                CustomLabelValue {
                    visible: walletInfo.encType === QPasswordData.Auth
                    text: qsTr("%1 seconds left").arg(timer.timeLeft.toFixed(0))
                    Layout.fillWidth: true
                }

                CustomProgressBar {
                    visible: walletInfo.encType === QPasswordData.Auth
                    Layout.minimumHeight: 6
                    Layout.preferredHeight: 6
                    Layout.maximumHeight: 6
                    Layout.bottomMargin: 10
                    Layout.fillWidth: true
                    to: duration
                    value: timer.timeLeft
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

