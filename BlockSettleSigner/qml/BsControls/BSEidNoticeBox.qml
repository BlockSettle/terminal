/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.9
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0

import "../StyledControls"
import "../BsStyles"


BSMessageBox {
    id: root
    title: qsTr("Notice!")
    customText: qsTr("Encrypting your wallet with Auth eID")

    customDetails: qsTr("Auth eID is a free-to-use mobile app that offers a convenient alternative to passwords. \
<br><br>Instead of manually selecting a password to encrypt your wallet’s Root Private Key (RPK), \
you can download Auth eID on your mobile device and encrypt your wallet with a data generated password secured by a pin or fingerprint.")

    acceptable: true
    cancelButtonVisible: false
    width: 410
    height: 540

    messageDialogContentItem: ColumnLayout {
        Layout.leftMargin: 72
        Layout.rightMargin: 2

        CustomLabel {
            text: qsTr("Important!")
            font.italic: true
            color: "white"
        }

        CustomLabel {
            id: warnText
            Layout.preferredWidth: root.width - 80
            font.italic: true

            text: qsTr("Auth eID is not your wallet backup. \
If you lose or damage your mobile device, or forget your selected pin, \
you will need restore your wallet with the RPK backup.")
        }

        CustomLabel {
            Layout.preferredWidth: root.width - 80
            onLinkActivated: Qt.openUrlExternally(link)

            text: qsTr("For more information, please consult \
<br><a href=\"https://static.autheid.com/download/getting_started.pdf\"><span style=\"color:white;\">Getting Started With Auth eID</span></a>\
<br><br>")

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

