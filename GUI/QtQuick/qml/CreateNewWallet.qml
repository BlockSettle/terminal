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
    SeedPhrase {
        id: seedPhrasePage
        visible: false
    }
    ImportSeedPhrase {
        id: importSeedPhrasePage
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

        Text {
            text: qsTr("<font color=\"lightgrey\"><h1>Terms and conditions</h1> Neque porro quisquam est qui dolorem ipsum quia dolor sit amet, consectetur, adipisci velit...</font>")
            font.pointSize: 10
        }
        CheckBox {
            id: checkboxTerms
            text: qsTr("<font color=\"lightgrey\">Agree with terms and conditions</font>")
            font.pointSize: 12
        }

        Row {
            spacing: 23

            Button {
                text: qsTr("Hardware Wallet")
                font.pointSize: 12
                enabled: false
            }
            Button {
                text: qsTr("Import Wallet")
                font.pointSize: 12
                enabled: checkboxTerms.checked
                onClicked: {
                    stack.push(importSeedPhrasePage)
                }
            }
            Button {
                text: qsTr("Create new")
                font.pointSize: 12
                enabled: checkboxTerms.checked
                onClicked: {
                    seedPhrasePage.phrase = bsApp.newSeedPhrase()
                    stack.push(seedPhrasePage)
                }
            }
        }
    }
}
