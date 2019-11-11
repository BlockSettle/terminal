import QtQuick 2.9
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0

import "../StyledControls"
import "../BsStyles"


BSMessageBox {
    id: root
    title: qsTr("Notice!")
    customText: qsTr("Signing with Auth eID")

    customDetails: qsTr("Encrypting your wallet with Auth eID \
<br><br>Auth eID is a free-to-use mobile app that offers a convenient alternative to passwords. \
<br><br>Instead of manually selecting a password to encrypt your wallet’s Root Private Key (RPK), \
you can encrypt your wallet with a data generated password secured by a pin or fingerprint on your mobile device. \
Auth eID is available to download for Android and iOS.")

    acceptable: true
    cancelButtonVisible: false
    width: 400

    messageDialogContentItem: ColumnLayout {
        Layout.leftMargin: 72
        Layout.rightMargin: 2

        CustomLabel {
            text: qsTr("Important!")
            font.bold: true
        }

        CustomLabel {
            id: warnText
            Layout.preferredWidth: root.width - 80
            text: qsTr("Auth eID is not a wallet backup. No wallet data is stored with Auth eID. \
<br><br>In the event that you lose or damage your mobile device, or forget the pin code you have set, \
you will need your RPK to restore your wallet. Therefore, you must maintain a safe copy of your RPK.")
        }

        CustomLabel {
            Layout.preferredWidth: root.width - 80
            onLinkActivated: Qt.openUrlExternally(link)

            text: qsTr("Note: user who want to trade through BlockSettle will need to go through ID verification using Auth eID. \
However, you may still use your manually selected password in order to sign transactions. \
<br><br>For more information, please consult:\
<br><a href=\"https://static.autheid.com/download/getting_started.pdf\"><span style=\"color:white;\">Getting Started With Auth eID</span></a>.")

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.NoButton
                cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
            }
        }
    }

    onAccepted: {
        if (cb.checked) {
            signerSettings.hideEidInfoBox = true
        }
    }
}

