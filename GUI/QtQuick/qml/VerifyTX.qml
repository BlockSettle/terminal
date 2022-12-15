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
    property var txSignRequest

    Column {
        spacing: 23
        anchors.fill: parent

        Button {
            icon.source: "qrc:/images/send_icon.png"
            onClicked: {
                stack.pop()
            }
        }
        GridLayout {
            columns: 2
            Label {
                text: qsTr("<font color=\"cyan\">Output address:</font>")
                font.pointSize: 12
            }
            Label {
                text: qsTr("<font color=\"lightgrey\">%1</font>").arg(txSignRequest.outputAddresses[0])
                font.pointSize: 12
            }
            Label {
                text: qsTr("<font color=\"cyan\">Output amount:</font>")
                font.pointSize: 12
            }
            Label {
                text: qsTr("<font color=\"lightgrey\">%1</font>").arg(txSignRequest.outputAmount)
                font.pointSize: 12
            }
            Label {
                text: qsTr("<font color=\"darkgrey\">Input amount:</font>")
                font.pointSize: 12
            }
            Label {
                text: qsTr("<font color=\"lightgrey\">%1</font>").arg(txSignRequest.inputAmount)
                font.pointSize: 12
            }
            Label {
                text: qsTr("<font color=\"darkgrey\">Return amount:</font>")
                font.pointSize: 12
            }
            Label {
                text: qsTr("<font color=\"lightgrey\">%1</font>").arg(txSignRequest.returnAmount)
                font.pointSize: 12
            }
            Label {
                text: qsTr("<font color=\"darkgrey\">Transaction fee:</font>")
                font.pointSize: 12
            }
            Label {
                text: qsTr("<font color=\"lightgrey\">%1</font>").arg(txSignRequest.fee)
                font.pointSize: 12
            }
            Label {
                text: qsTr("<font color=\"darkgrey\">Transaction size:</font>")
                font.pointSize: 12
            }
            Label {
                text: qsTr("<font color=\"lightgrey\">%1</font>").arg(txSignRequest.txSize)
                font.pointSize: 12
            }
            Label {
                text: qsTr("<font color=\"darkgrey\">Fee per byte:</font>")
                font.pointSize: 12
            }
            Label {
                text: qsTr("<font color=\"lightgrey\">%1</font>").arg(txSignRequest.feePerByte)
                font.pointSize: 12
            }
        }
        TextInput {
            id: password
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
        Button {
            width: 900
            text: qsTr("Broadcast")
            font.pointSize: 14
            enabled: txSignRequest.isValid && password.text.length

            onClicked: {
                bsApp.signAndBroadcast(txSignRequest, password.text)
                stack.pop()
                stack.pop()
                password.text = ""
            }
        }
    }
}
