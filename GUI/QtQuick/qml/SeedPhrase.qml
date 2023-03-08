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
//import "BsControls"
//import "BsDialogs"
//import "js/helper.js" as JsHelper

Item {
    property var phrase: null

    VerifySeedPhrase {
        id: verifySeedPhrase
        visible: false
    }

    Column {
        spacing: 32
        anchors.fill: parent

        Button {
            icon.source: "qrc:/images/send_icon.png"
            onClicked: {
                stack.pop()
            }
        }
        Label {
            text: qsTr("<font color=\"white\">Create new wallet</font>")
            font.pointSize: 14
        }
        Label {
            text: qsTr("<font color=\"lightgrey\">Write down and store your 12 word seed someplace safe and offline</font>")
            font.pointSize: 10
        }

        Row {
            spacing: 23

            TextEdit {
                id: word1
                width: 250
                height: 32
                text: phrase !== null ? phrase[0] : ""
                color: 'darkgrey'
                font.pointSize: 12
                enabled: false
            }
            TextEdit {
                id: word2
                width: 250
                height: 32
                font.pointSize: 12
                color: 'darkgrey'
                text: phrase !== null ? phrase[1] : ""
                enabled: false
            }
            TextEdit {
                id: word3
                width: 250
                height: 32
                font.pointSize: 12
                color: 'darkgrey'
                text: phrase !== null ? phrase[2] : ""
                enabled: false
            }
        }
        Row {
            spacing: 23

            TextEdit {
                id: word4
                width: 250
                height: 32
                font.pointSize: 12
                color: 'darkgrey'
                text: phrase !== null ? phrase[3] : ""
                enabled: false
            }
            TextEdit {
                id: word5
                width: 250
                height: 32
                font.pointSize: 12
                color: 'darkgrey'
                text: phrase !== null ? phrase[4] : ""
                enabled: false
            }
            TextEdit {
                id: word6
                width: 250
                height: 32
                font.pointSize: 12
                color: 'darkgrey'
                text: phrase !== null ? phrase[5] : ""
                enabled: false
            }
        }
        Row {
            spacing: 23

            TextEdit {
                id: word7
                width: 250
                height: 32
                font.pointSize: 12
                color: 'darkgrey'
                text: phrase !== null ? phrase[6] : ""
                enabled: false
            }
            TextEdit {
                id: word8
                width: 250
                height: 32
                font.pointSize: 12
                color: 'darkgrey'
                text: phrase !== null ? phrase[7] : ""
                enabled: false
            }
            TextEdit {
                id: word9
                width: 250
                height: 32
                font.pointSize: 12
                color: 'darkgrey'
                text: phrase !== null ? phrase[8] : ""
                enabled: false
            }
        }
        Row {
            spacing: 23

            TextEdit {
                id: word10
                width: 250
                height: 32
                font.pointSize: 12
                color: 'darkgrey'
                text: phrase !== null ? phrase[9] : ""
                enabled: false
            }
            TextEdit {
                id: word11
                width: 250
                height: 32
                font.pointSize: 12
                color: 'darkgrey'
                text: phrase !== null ? phrase[10] : ""
                enabled: false
            }
            TextEdit {
                id: word12
                width: 250
                height: 32
                font.pointSize: 12
                color: 'darkgrey'
                text: phrase !== null ? phrase[11] : ""
                enabled: false
            }
        }
        Row {
            spacing: 50
            Button {
                width: 450
                text: qsTr("Copy Seed")
                font.pointSize: 12
                onClicked: {
                    bsApp.copySeedToClipboard(phrase)
                }
            }
            Button {
                width: 450
                text: qsTr("Continue")
                font.pointSize: 12
                onClicked: {
                    verifySeedPhrase.phrase = phrase
                    stack.push(verifySeedPhrase)
                }
            }
        }
    }
}
