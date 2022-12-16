/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
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

        Row {
            spacing: 100
            Button {
                icon.source: "qrc:/images/send_icon.png"
                onClicked: {
                    stack.pop()
                }
            }
            Label {
                text: qsTr("<font color=\"lightgrey\">Sign Transaction</font>")
                font.pointSize: 14
            }
        }
        Row {
            spacing: 23

            TableView {
                width: 480
                height: 250
                columnSpacing: 1
                rowSpacing: 1
                clip: true
                ScrollIndicator.horizontal: ScrollIndicator { }
                ScrollIndicator.vertical: ScrollIndicator { }
                model: addressListModel
                delegate: Rectangle {
                    implicitWidth: 120
                    implicitHeight: 20
                    border.color: "black"
                    border.width: 1
                    color: heading ? 'black' : 'darkslategrey'
                    Text {
                        text: tabledata
                        font.pointSize: heading ? 8 : 10
                        color: heading ? 'darkgrey' : 'lightgrey'
                        anchors.centerIn: parent
                    }
                }
            }

            TableView {
                width: 480
                height: 250
                columnSpacing: 1
                rowSpacing: 1
                clip: true
                ScrollIndicator.horizontal: ScrollIndicator { }
                ScrollIndicator.vertical: ScrollIndicator { }
                model: addressListModel
                delegate: Rectangle {
                    implicitWidth: 120
                    implicitHeight: 20
                    border.color: "black"
                    border.width: 1
                    color: heading ? 'black' : 'darkslategrey'
                    Text {
                        text: tabledata
                        font.pointSize: heading ? 8 : 10
                        color: heading ? 'darkgrey' : 'lightgrey'
                        anchors.centerIn: parent
                    }
                }
            }
        }

        GridLayout {
            columns: 4
            columnSpacing: 50
            Label {
                text: qsTr("<font color=\"darkgrey\">Input amount:</font>")
                font.pointSize: 12
            }
            Label {
                text: qsTr("<font color=\"lightgrey\">%1</font>").arg(txSignRequest.inputAmount)
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
                text: qsTr("<font color=\"darkgrey\">Return amount:</font>")
                font.pointSize: 12
            }
            Label {
                text: qsTr("<font color=\"lightgrey\">%1</font>").arg(txSignRequest.returnAmount)
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
            Label {
                text: qsTr("<font color=\"darkgrey\">Transaction fee:</font>")
                font.pointSize: 12
            }
            Label {
                text: qsTr("<font color=\"lightgrey\">%1</font>").arg(txSignRequest.fee)
                font.pointSize: 12
            }
            Label {
                text: qsTr("<font color=\"darkgrey\">Total spent:</font>")
                font.pointSize: 12
            }
            Label {
                text: qsTr("<font color=\"lightgrey\">%1</font>").arg(txSignRequest.outputAmount)
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
                stack.pop()
                password.text = ""
            }
        }
    }
}
