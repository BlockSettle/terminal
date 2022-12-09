/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2
import QtQuick.Controls 2.9
import QtQuick.Layouts 1.3
import QtQml.Models 2

import "StyledControls"
import "BsStyles"

Item {
    property var phrase
    property int popLevel: 3
    property bool isImport: false

    Column {
        spacing: 23
        anchors.fill: parent

        Button {
            icon.source: "qrc:/images/send_icon.png"
            onClicked: {
                stack.pop()
            }
        }
        Label {
            text: qsTr("<font color=\"white\">Set password</font>")
            font.pointSize: 14
        }

        TextInput {
            id: walletName
            width: 500
            height: 32
            color: 'lightgrey'
            font.pointSize: 14
            horizontalAlignment: TextEdit.AlignHCenter
            verticalAlignment: TextEdit.AlignVCenter
            Text {
                text: qsTr("Wallet name")
                font.pointSize: 6
                color: 'darkgrey'
                anchors.left: parent
                anchors.top: parent
            }
        }

        TextInput {
            id: pass1
            width: 500
            height: 32
            color: 'lightgrey'
            font.pointSize: 14
            horizontalAlignment: TextEdit.AlignHCenter
            verticalAlignment: TextEdit.AlignVCenter
            echoMode: TextInput.Password
            passwordCharacter: '*'
            Text {
                text: qsTr("Password")
                font.pointSize: 6
                color: 'darkgrey'
                anchors.left: parent
                anchors.top: parent
            }
        }
        TextInput {
            id: pass2
            width: 500
            height: 32
            color: 'lightgrey'
            font.pointSize: 14
            horizontalAlignment: TextEdit.AlignHCenter
            verticalAlignment: TextEdit.AlignVCenter
            echoMode: TextInput.Password
            passwordCharacter: '*'
            Text {
                text: qsTr("Confirm password")
                font.pointSize: 6
                color: 'darkgrey'
                anchors.left: parent
                anchors.top: parent
            }
        }

        Button {
            width: 900
            text: qsTr("Confirm")
            font.pointSize: 14
            enabled: pass1.text.length && pass2.text.length && (pass1.text == pass2.text)

            onClicked: {
                if (isImport) {
                    bsApp.importWallet(walletName.text, phrase, pass1.text)
                }
                else {
                    bsApp.createWallet(walletName.text, phrase, pass1.text)
                }
                for (var i = 0; i < popLevel; i++) {
                    stack.pop()
                }
            }
        }
    }
}
