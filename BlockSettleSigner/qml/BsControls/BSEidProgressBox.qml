/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.9
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0
import com.blocksettle.AutheIDClient 1.0

import "../StyledControls"
import "../BsStyles"

CustomTitleDialogWindow {
    id: root

    property int timeout: 120
    property int countDown: 120
    property string email
    property string walletId
    property string walletName
    property int requestType

    acceptable: false
    rejectable: true
    onEnterPressed: {
        rejectAnimated()
    }

    width: 350
    height: 210 + headerPanelHeight

    title: qsTr("Sign with Auth eID")

    onRequestTypeChanged: {
        switch (requestType) {
        case AutheIDClient.ActivateWallet:
            root.title = "Activate Auth eID encryption"
            break
        case AutheIDClient.DeactivateWallet:
            root.title = "Deactivate Auth eID encryption"
            break
        case AutheIDClient.BackupWallet:
            root.title = qsTr("Export Wallet")
            break
        case AutheIDClient.SignWallet:
            break
        default:
            root.title = "Sign with Auth eID"
            break
        }
    }

    Timer {
        id: authTimer
        running: true
        interval: 1000
        repeat: true
        onTriggered: {
            countDown--
            if (countDown === 0) {
                authTimer.stop()
                rejectAnimated()
            }
        }
    }

    cContentItem: ColumnLayout {
        Layout.fillWidth: true
        ColumnLayout {
            spacing: 0
            Layout.topMargin: 5
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomHeader {
                text: qsTr("Wallet Details")
                Layout.fillWidth: true
                Layout.preferredHeight: 25
            }

            RowLayout {
                spacing: 5
                Layout.topMargin: 5
                Layout.fillWidth: true

                CustomLabel {
                    Layout.minimumWidth: 110
                    Layout.preferredWidth: 110
                    Layout.maximumWidth: 110
                    Layout.fillWidth: true
                    text: qsTr("Wallet name")
                }
                CustomLabel {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 110
                    text: walletInfo.name
                }
            }

            RowLayout {
                spacing: 5
                Layout.fillWidth: true

                CustomLabel {
                    Layout.minimumWidth: 110
                    Layout.preferredWidth: 110
                    Layout.maximumWidth: 110
                    Layout.fillWidth: true
                    text: qsTr("Wallet ID")
                }
                CustomLabel {
                    Layout.fillWidth: true
                    text: walletInfo.walletId
                }
            }

            RowLayout {
                spacing: 5
                Layout.fillWidth: true

                CustomLabel {
                    Layout.minimumWidth: 110
                    Layout.preferredWidth: 110
                    Layout.maximumWidth: 110
                    Layout.fillWidth: true
                    text: qsTr("Auth eID")
                }
                CustomLabel {
                    Layout.fillWidth: true
                    text: walletInfo.email()
                }
            }

            CustomProgressBar {
                id: authProgress
                Layout.fillWidth: true
                Layout.topMargin: 5
                from: 0.0
                to: timeout
                value: countDown
            }

            CustomLabel {
                id: countDownLabel
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                color: BSStyle.progressBarColor
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("%1 seconds left").arg(countDown)
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
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.margins: 5
                text: qsTr("Cancel")
                onClicked: {
                    rejectAnimated()
                }
            }
        }
    }
}

