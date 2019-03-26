import QtQuick 2.9
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0

import "../StyledControls"
import "../BsStyles"

CustomTitleDialogWindow {
    id: root

    property int timeout: 120
    property int countDown: 120
    property string email
    property string walletId
    property string walletName

    property string centralText : qsTr("Activate Auth eID signing\n Wallet ID: %1\n Wallet Name: %2").arg(walletId).arg(walletName)
    acceptable: false
    rejectable: true
    onEnterPressed: {
        rejectAnimated()
    }

    width: 350

    title: qsTr("Sign With Auth eID")

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
            Layout.preferredWidth: root.width

            CustomLabelValue{
                id: labelText_
                text: centralText
                lineHeight: 1.5
                padding: 0
                Layout.preferredWidth: root.width
                horizontalAlignment: Text.AlignHCenter
                color: BSStyle.dialogTitleWhiteColor
                Layout.minimumHeight: 40
            }

            CustomLabel{
                id: labelDetails_
                text: email
                padding: 15
                //textFormat: Text.RichText
                Layout.preferredWidth: root.width
                horizontalAlignment: Text.AlignHCenter

                onLinkActivated: Qt.openUrlExternally(link)
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                }
            }

            CustomProgressBar {
                id: authProgress
                Layout.fillWidth: true
                Layout.topMargin: 5
                Layout.leftMargin: 10
                Layout.rightMargin: 10
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

