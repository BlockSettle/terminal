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

    WalletNamePass {
        id: walletNamePass
        visible: false
    }

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
            text: qsTr("<font color=\"white\">Verify your seed</font>")
            font.pointSize: 14
        }

        TextInput {
            id: word1
            property bool isValid: (phrase[0] === text)
            width: 500
            height: 32
            color: 'lightgrey'
            font.pointSize: 12
            horizontalAlignment: TextEdit.AlignHCenter
            verticalAlignment: TextEdit.AlignVCenter
            Text {
                text: "1"
                font.pointSize: 6
                color: 'darkgrey'
                anchors.left: parent
                anchors.top: parent
            }
        }
        TextInput {
            id: word6
            property bool isValid: (phrase[5] === text)
            width: 500
            height: 32
            color: 'lightgrey'
            font.pointSize: 12
            horizontalAlignment: TextEdit.AlignHCenter
            verticalAlignment: TextEdit.AlignVCenter
            Text {
                text: "6"
                font.pointSize: 6
                color: 'darkgrey'
                anchors.left: parent
                anchors.top: parent
            }
        }
        TextInput {
            id: word8
            property bool isValid: (phrase[7] === text)
            width: 500
            height: 32
            color: 'lightgrey'
            font.pointSize: 12
            horizontalAlignment: TextEdit.AlignHCenter
            verticalAlignment: TextEdit.AlignVCenter
            Text {
                text: "8"
                font.pointSize: 6
                color: 'darkgrey'
                anchors.left: parent
                anchors.top: parent
            }
        }
        TextInput {
            id: word11
            property bool isValid: (phrase[10] === text)
            width: 500
            height: 32
            color: 'lightgrey'
            font.pointSize: 12
            horizontalAlignment: TextEdit.AlignHCenter
            verticalAlignment: TextEdit.AlignVCenter
            Text {
                text: "11"
                font.pointSize: 6
                color: 'darkgrey'
                anchors.left: parent
                anchors.top: parent
            }
        }

        Label {
            id: verifResult
            height: 32
            color: 'red'
            text: (word1.isValid && word6.isValid && word8.isValid && word11.isValid)
                ? "" : qsTr("Your words are wrong")
        }

        Row {
            spacing: 50
            Button {
                width: 450
                text: qsTr("Skip")
                font.pointSize: 12
                onClicked: {    // TODO: show confirmation or add a checkbox to confirm
                    walletNamePass.phrase = phrase
                    walletNamePass.popLevel = 4
                    stack.push(walletNamePass)
                }
            }
            Button {
                width: 450
                text: qsTr("Continue")
                font.pointSize: 12
                enabled: word1.isValid && word6.isValid && word8.isValid && word11.isValid
                onClicked: {
                    walletNamePass.phrase = phrase
                    walletNamePass.popLevel = 4
                    stack.push(walletNamePass)
                }
            }
        }
    }
}
