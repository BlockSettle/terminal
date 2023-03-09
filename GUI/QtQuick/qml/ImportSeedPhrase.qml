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
    property var phrase

    WalletNamePass {
        id: walletNamePassPage
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
            text: qsTr("<font color=\"white\">Import wallet</font>")
            font.pointSize: 14
        }

        Row {
            spacing: 16
            Label {
                text: qsTr("<font color=\"darkgrey\">Seed phrase type</font>")
                font.pointSize: 10
            }
            RadioButton {
                id: rb12Words
                checked: true
                text: qsTr("<font color=\"darkgrey\">12 words</font>")
                font.pointSize: 10
            }
            RadioButton {
                id: rb24Words
                checked: false
                text: qsTr("<font color=\"darkgrey\">24 words</font>")
                font.pointSize: 10
            }
        }

        Row {
            spacing: 23

            TextInput {
                id: word1
                width: 250
                height: 23
                color: 'lightgrey'
                font.pointSize: 12
                horizontalAlignment: TextEdit.AlignHCenter
                verticalAlignment: TextEdit.AlignVCenter
                Text {
                    text: "1"
                    font.pointSize: 6
                    color: 'darkgrey'
                }
            }
            TextInput {
                id: word2
                width: 250
                height: 23
                font.pointSize: 12
                color: 'lightgrey'
                horizontalAlignment: TextEdit.AlignHCenter
                verticalAlignment: TextEdit.AlignVCenter
                Text {
                    text: "2"
                    font.pointSize: 6
                    color: 'darkgrey'
                }
            }
            TextInput {
                id: word3
                width: 250
                height: 23
                font.pointSize: 12
                color: 'lightgrey'
                horizontalAlignment: TextEdit.AlignHCenter
                verticalAlignment: TextEdit.AlignVCenter
                Text {
                    text: "3"
                    font.pointSize: 6
                    color: 'darkgrey'
                }
            }
        }
        Row {
            spacing: 23

            TextInput {
                id: word4
                width: 250
                height: 23
                font.pointSize: 12
                color: 'lightgrey'
                horizontalAlignment: TextEdit.AlignHCenter
                verticalAlignment: TextEdit.AlignVCenter
                Text {
                    text: "4"
                    font.pointSize: 6
                    color: 'darkgrey'
                }
            }
            TextInput {
                id: word5
                width: 250
                height: 23
                font.pointSize: 12
                color: 'lightgrey'
                horizontalAlignment: TextEdit.AlignHCenter
                verticalAlignment: TextEdit.AlignVCenter
                Text {
                    text: "5"
                    font.pointSize: 6
                    color: 'darkgrey'
                }
            }
            TextInput {
                id: word6
                width: 250
                height: 23
                font.pointSize: 12
                color: 'lightgrey'
                horizontalAlignment: TextEdit.AlignHCenter
                verticalAlignment: TextEdit.AlignVCenter
                Text {
                    text: "6"
                    font.pointSize: 6
                    color: 'darkgrey'
                }
            }
        }
        Row {
            spacing: 23

            TextInput {
                id: word7
                width: 250
                height: 23
                font.pointSize: 12
                color: 'lightgrey'
                horizontalAlignment: TextEdit.AlignHCenter
                verticalAlignment: TextEdit.AlignVCenter
                Text {
                    text: "7"
                    font.pointSize: 6
                    color: 'darkgrey'
                }
            }
            TextInput {
                id: word8
                width: 250
                height: 23
                font.pointSize: 12
                color: 'lightgrey'
                horizontalAlignment: TextEdit.AlignHCenter
                verticalAlignment: TextEdit.AlignVCenter
                Text {
                    text: "8"
                    font.pointSize: 6
                    color: 'darkgrey'
                }
            }
            TextInput {
                id: word9
                width: 250
                height: 23
                font.pointSize: 12
                color: 'lightgrey'
                horizontalAlignment: TextEdit.AlignHCenter
                verticalAlignment: TextEdit.AlignVCenter
                Text {
                    text: "9"
                    font.pointSize: 6
                    color: 'darkgrey'
                }
            }
        }
        Row {
            spacing: 23

            TextInput {
                id: word10
                width: 250
                height: 23
                font.pointSize: 12
                color: 'lightgrey'
                horizontalAlignment: TextEdit.AlignHCenter
                verticalAlignment: TextEdit.AlignVCenter
                Text {
                    text: "10"
                    font.pointSize: 6
                    color: 'darkgrey'
                }
            }
            TextInput {
                id: word11
                width: 250
                height: 23
                font.pointSize: 12
                color: 'lightgrey'
                horizontalAlignment: TextEdit.AlignHCenter
                verticalAlignment: TextEdit.AlignVCenter
                Text {
                    text: "11"
                    font.pointSize: 6
                    color: 'darkgrey'
                }
            }
            TextInput {
                id: word12
                width: 250
                height: 23
                font.pointSize: 12
                color: 'lightgrey'
                horizontalAlignment: TextEdit.AlignHCenter
                verticalAlignment: TextEdit.AlignVCenter
                Text {
                    text: "12"
                    font.pointSize: 6
                    color: 'darkgrey'
                }
            }
        }
        Row {
            spacing: 23
            visible: rb24Words.checked

            TextInput {
                id: word13
                width: 250
                height: 23
                font.pointSize: 12
                color: 'lightgrey'
                horizontalAlignment: TextEdit.AlignHCenter
                verticalAlignment: TextEdit.AlignVCenter
                Text {
                    text: "13"
                    font.pointSize: 6
                    color: 'darkgrey'
                }
            }
            TextInput {
                id: word14
                width: 250
                height: 23
                font.pointSize: 12
                color: 'lightgrey'
                horizontalAlignment: TextEdit.AlignHCenter
                verticalAlignment: TextEdit.AlignVCenter
                Text {
                    text: "14"
                    font.pointSize: 6
                    color: 'darkgrey'
                }
            }
            TextInput {
                id: word15
                width: 250
                height: 23
                font.pointSize: 12
                color: 'lightgrey'
                horizontalAlignment: TextEdit.AlignHCenter
                verticalAlignment: TextEdit.AlignVCenter
                Text {
                    text: "15"
                    font.pointSize: 6
                    color: 'darkgrey'
                }
            }
        }
        Row {
            spacing: 23
            visible: rb24Words.checked

            TextInput {
                id: word16
                width: 250
                height: 23
                font.pointSize: 12
                color: 'lightgrey'
                horizontalAlignment: TextEdit.AlignHCenter
                verticalAlignment: TextEdit.AlignVCenter
                Text {
                    text: "16"
                    font.pointSize: 6
                    color: 'darkgrey'
                }
            }
            TextInput {
                id: word17
                width: 250
                height: 23
                font.pointSize: 12
                color: 'lightgrey'
                horizontalAlignment: TextEdit.AlignHCenter
                verticalAlignment: TextEdit.AlignVCenter
                Text {
                    text: "17"
                    font.pointSize: 6
                    color: 'darkgrey'
                }
            }
            TextInput {
                id: word18
                width: 250
                height: 23
                font.pointSize: 12
                color: 'lightgrey'
                horizontalAlignment: TextEdit.AlignHCenter
                verticalAlignment: TextEdit.AlignVCenter
                Text {
                    text: "18"
                    font.pointSize: 6
                    color: 'darkgrey'
                }
            }
        }
        Row {
            spacing: 23
            visible: rb24Words.checked

            TextInput {
                id: word19
                width: 250
                height: 23
                font.pointSize: 12
                color: 'lightgrey'
                horizontalAlignment: TextEdit.AlignHCenter
                verticalAlignment: TextEdit.AlignVCenter
                Text {
                    text: "19"
                    font.pointSize: 6
                    color: 'darkgrey'
                }
            }
            TextInput {
                id: word20
                width: 250
                height: 23
                font.pointSize: 12
                color: 'lightgrey'
                horizontalAlignment: TextEdit.AlignHCenter
                verticalAlignment: TextEdit.AlignVCenter
                Text {
                    text: "20"
                    font.pointSize: 6
                    color: 'darkgrey'
                }
            }
            TextInput {
                id: word21
                width: 250
                height: 23
                font.pointSize: 12
                color: 'lightgrey'
                horizontalAlignment: TextEdit.AlignHCenter
                verticalAlignment: TextEdit.AlignVCenter
                Text {
                    text: "21"
                    font.pointSize: 6
                    color: 'darkgrey'
                }
            }
        }
        Row {
            spacing: 23
            visible: rb24Words.checked

            TextInput {
                id: word22
                width: 250
                height: 23
                font.pointSize: 12
                color: 'lightgrey'
                horizontalAlignment: TextEdit.AlignHCenter
                verticalAlignment: TextEdit.AlignVCenter
                Text {
                    text: "22"
                    font.pointSize: 6
                    color: 'darkgrey'
                }
            }
            TextInput {
                id: word23
                width: 250
                height: 23
                font.pointSize: 12
                color: 'lightgrey'
                horizontalAlignment: TextEdit.AlignHCenter
                verticalAlignment: TextEdit.AlignVCenter
                Text {
                    text: "23"
                    font.pointSize: 6
                    color: 'darkgrey'
                }
            }
            TextInput {
                id: word24
                width: 250
                height: 23
                font.pointSize: 12
                color: 'lightgrey'
                horizontalAlignment: TextEdit.AlignHCenter
                verticalAlignment: TextEdit.AlignVCenter
                Text {
                    text: "24"
                    font.pointSize: 6
                    color: 'darkgrey'
                }
            }
        }

        Button {
            width: 900
            text: qsTr("Import")
            font.pointSize: 14
            enabled: word1.text.length && word2.text.length && word3.text.length
                && word4.text.length && word5.text.length && word6.text.length
                && word7.text.length && word8.text.length && word9.text.length
                && word10.text.length && word11.text.length && word12.text.length
                && (rb24Words.checked ? (word13.text.length && word14.text.length
                    && word15.text.length && word16.text.length && word17.text.length
                    && word18.text.length && word19.text.length && word20.text.length
                    && word21.text.length && word22.text.length && word23.text.length
                    && word24.text.length) : true)

            onClicked: {
                phrase = [ word1.text, word2.text, word3.text, word4.text,
                    word5.text, word6.text, word7.text, word8.text, word9.text,
                    word10.text, word11.text, word12.text]
                if (rb24Words.checked) {
                    phrase.append(word13.text)
                    phrase.append(word14.text)
                    phrase.append(word15.text)
                    phrase.append(word16.text)
                    phrase.append(word17.text)
                    phrase.append(word18.text)
                    phrase.append(word19.text)
                    phrase.append(word20.text)
                    phrase.append(word21.text)
                    phrase.append(word22.text)
                    phrase.append(word23.text)
                    phrase.append(word24.text)

                }
                walletNamePassPage.phrase = phrase
                walletNamePassPage.isImport = true
                stack.push(walletNamePassPage)
            }
        }
    }
}
