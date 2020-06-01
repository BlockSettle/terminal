/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.9
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.11

import com.blocksettle.AutheIDClient 1.0

import "../StyledControls"
import "../BsStyles"

// used for password confirmation in wallet create/import dialog
BSPasswordInput {
    id: root

    property string passwordToCheck
    property bool passwordCorrect: passwordInput.text.length !== 0 && (passwordToCheck.length ===0 || passwordInput.text === passwordToCheck)

    title: qsTr("Notice!")
    btnAccept.text: qsTr("Continue")
    btnAccept.enabled: passwordCorrect
    autheIDSignType: AutheIDClient.CreateAuthLeaf

    decryptHeaderText: qsTr("Check Password")

    CustomHeader {
        text: qsTr("Please take care of your assets!")
        Layout.fillWidth: true
        Layout.preferredHeight: 25
        Layout.topMargin: 5
        Layout.leftMargin: 10
        Layout.rightMargin: 10
    }

    CustomLabel{
        id: labelDetails_
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
}

