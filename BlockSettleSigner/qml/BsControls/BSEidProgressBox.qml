import QtQuick 2.9
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0

import "../StyledControls"
import "../BsStyles"

BSMessageBox {
    id: root

    property int timeout: 120
    property int countDown: 120

    acceptable: false
    acceptButtonVisible: false

    title: qsTr("Sign With Auth eID")
    customText: qsTr("")
    labelText.color: BSStyle.dialogTitleGreenColor

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

    messageDialogContentItem: ColumnLayout {
        Layout.fillWidth: true
        CustomLabel {
            id: countDownLabel
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            color: BSStyle.progressBarColor
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("%1 seconds left").arg(countDown)
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
    }
}

