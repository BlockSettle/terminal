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
    id: settings

    Column {
        spacing: 50
        anchors.fill: parent

        Button {
            icon.source: "qrc:/images/send_icon.png"
            onClicked: {
                stack.pop()
            }
        }
        CheckBox {
            id: cbGeneral
            text: qsTr("<font color=\"lightgrey\">General &gt;</font>")
            font.pointSize: 14
            onCheckStateChanged: {
                if (checked) {
                    cbNetwork.checked = false
                    cbAbout.checked = false
                }
            }
        }
        GridLayout {
            columns: 2
            visible: cbGeneral.checked

            Label {
                text: qsTr("<font color=\"lightgrey\">Log file:</font>")
                font.pointSize: 12
            }
            TextInput {
                id: mainLog
                width: 500
                height: 32
                color: 'lightgrey'
                text: bsApp.settingLogFile
                font.pointSize: 12
                horizontalAlignment: TextEdit.AlignHCenter
                verticalAlignment: TextEdit.AlignVCenter
            }
            Label {
                text: qsTr("<font color=\"lightgrey\">Messages log file:</font>")
                font.pointSize: 12
            }
            TextInput {
                id: msgLog
                width: 500
                height: 32
                color: 'lightgrey'
                text: bsApp.settingMsgLogFile
                font.pointSize: 12
                horizontalAlignment: TextEdit.AlignHCenter
                verticalAlignment: TextEdit.AlignVCenter
            }
            Label {
                text: " "
                font.pointSize: 12
            }
            CheckBox {
                id: cbAdvTX
                width: 500
                height: 32
                text: qsTr("<font color=\"lightgrey\">Advanced TX dialog by default</font>")
                font.pointSize: 12
                checked: bsApp.settingAdvancedTX
            }
        }

        CheckBox {
            id: cbNetwork
            text: qsTr("<font color=\"lightgrey\">Network &gt;</font>")
            font.pointSize: 14
            onCheckStateChanged: {
                if (checked) {
                    cbGeneral.checked = false
                    cbAbout.checked = false
                }
            }
        }
        GridLayout {
            columns: 2
            visible: cbNetwork.checked
            Label {
                text: qsTr("<font color=\"lightgrey\">Environment:</font>")
                font.pointSize: 12
            }
            ComboBox {
                id: cbEnvironments
                model: bsApp.settingEnvironments
                font.pointSize: 12
                currentIndex: bsApp.settingEnvironment
                onCurrentIndexChanged: {
                    bsApp.settingEnvironment = currentIndex
                }
            }
            Label {
                text: qsTr("<font color=\"lightgrey\">Armory host:</font>")
                font.pointSize: 12
            }
            TextInput {
                id: armoryHost
                width: 500
                height: 32
                color: 'lightgrey'
                text: bsApp.settingArmoryHost
                font.pointSize: 12
                horizontalAlignment: TextEdit.AlignHCenter
                verticalAlignment: TextEdit.AlignVCenter
                onTextChanged: {
                    bsApp.settingArmoryHost = text
                }
            }
            Label {
                text: qsTr("<font color=\"lightgrey\">Armory port:</font>")
                font.pointSize: 12
            }
            TextInput {
                id: armoryPort
                width: 500
                height: 32
                color: 'lightgrey'
                text: bsApp.settingArmoryPort
                font.pointSize: 12
                horizontalAlignment: TextEdit.AlignHCenter
                verticalAlignment: TextEdit.AlignVCenter
                onTextChanged: {
                    bsApp.settingArmoryPort = text
                }
            }
        }
        CheckBox {
            id: cbAbout
            text: qsTr("<font color=\"lightgrey\">About &gt;</font>")
            font.pointSize: 14
            onCheckStateChanged: {
                if (checked) {
                    cbGeneral.checked = false
                    cbNetwork.checked = false
                }
            }
        }
    }
}
