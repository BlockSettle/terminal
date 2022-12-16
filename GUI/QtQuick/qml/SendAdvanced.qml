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
    property var recvAddress
    property int walletIdx
    property var sendAmount
    property var comment
    id: send

    VerifyAdvancedTX {
        id: verifySignTX
        visible: false
    }

    Column {
        spacing: 23
        anchors.fill: parent

        Row {
            Button {
                text: qsTr("Simple")
                font.pointSize: 12
                onClicked: {
                    stack.pop()
                }
            }
            Text {
                text: qsTr("<font color=\"white\">Send Bitcoin</font>")
                font.pointSize: 14
            }
        }

        Row {
            spacing: 23
            Column {
                spacing: 12
                Label {
                    text: qsTr("<font color=\"white\">Inputs</font>")
                    font.pointSize: 14
                }
                Row {
                    ComboBox {
                        id: sendWalletsComboBox
                        objectName: "sendWalletsComboBox"
                        model: bsApp.walletsList
                        currentIndex: walletIdx
                        font.pointSize: 12
                        enabled: (bsApp.walletsList.length > 1)
                        width: 350
                    }
                    Label {
                        text: qsTr("<font color=\"darkgrey\">%1 BTC</font>").arg(bsApp.totalBalance)
                        font.pointSize: 12
                    }
                }
                TextInput {
                    id: fees
                    width: 400
                    height: 32
                    color: 'lightgrey'
                    font.pointSize: 14
                    horizontalAlignment: TextEdit.AlignHCenter
                    verticalAlignment: TextEdit.AlignVCenter
                    Text {
                        text: qsTr("Fee Suggestions")
                        font.pointSize: 6
                        color: 'darkgrey'
                        anchors.left: parent
                        anchors.top: parent
                    }
                    Text {
                        text: qsTr("s/b")
                        font.pointSize: 10
                        color: 'darkgrey'
                        anchors.right: parent
                        anchors.horizontalCenter: parent
                    }
                }
                Row {
                    spacing: 23
                    Label {
                        text: qsTr("<font color=\"darkgrey\">Input Addresses</font>")
                        font.pointSize: 10
                    }
                    CheckBox {
                        id: cbSelectInputs
                        font.pointSize: 10
                        text: qsTr("<font color=\"cyan\">Select Inputs</font>")
                    }
                    Label {
                        text: qsTr("<font color=\"darkgrey\">#Tx: 0</font>")
                        font.pointSize: 10
                    }
                    Label {
                        text: qsTr("<font color=\"darkgrey\">Balance (BTC): %1</font>").arg(bsApp.totalBalance)
                        font.pointSize: 10
                    }
                }
                TableView {
                    width: 480
                    height: 250
                    columnSpacing: 1
                    rowSpacing: 1
                    visible: cbSelectInputs.checked
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
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                if (!heading) {
                                    //TODO: select
                                }
                            }
                        }
                    }
                }
            }

            Column {
                spacing: 12
                Row {
                    Label {
                        text: qsTr("<font color=\"white\">Outputs</font>")
                        font.pointSize: 14
                    }
                    Button {
                        text: "+"
                        font.pointSize: 14
                        onClicked: {
                        }
                    }
                }
                TextInput {
                    id: outAddr0
                    width: 400
                    height: 32
                    text: recvAddress
                    color: 'lightgrey'
                    font.pointSize: 14
                    horizontalAlignment: TextEdit.AlignHCenter
                    verticalAlignment: TextEdit.AlignVCenter
                    Text {
                        text: qsTr("Receiver address")
                        font.pointSize: 6
                        color: 'darkgrey'
                        anchors.left: parent
                        anchors.top: parent
                    }
                }
                Row {
                    TextInput {
                        id: amount0
                        width: 400
                        height: 32
                        text: sendAmount
                        color: 'lightgrey'
                        font.pointSize: 14
                        horizontalAlignment: TextEdit.AlignHCenter
                        verticalAlignment: TextEdit.AlignVCenter
                        Text {
                            text: qsTr("Amount")
                            font.pointSize: 6
                            color: 'darkgrey'
                            anchors.left: parent
                            anchors.top: parent
                        }
                        Text {
                            text: qsTr("BTC")
                            font.pointSize: 10
                            color: 'darkgrey'
                            anchors.right: parent
                            anchors.horizontalCenter: parent
                        }
                    }
                    Button {
                        text: qsTr("MAX")
                        font.pointSize: 14
                    }
                }
                TableView {
                    width: 480
                    height: 200
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
        }


        TextEdit {
            id: txComment
            text: comment
            width: 900
            height: 50
            color: 'lightgrey'
            verticalAlignment: TextEdit.AlignVCenter
            font.pointSize: 12
            Text {
                text: qsTr("Comment")
                color: 'darkgrey'
                anchors.left: parent
                anchors.top: parent
                font.pointSize: 10
            }
        }

        Button {
            width: 900
            text: qsTr("Continue")
            font.pointSize: 14
            enabled: recvAddress.text.length && (parseFloat(amount.text) >= 0.00001)
                && (parseFloat(fees.text) >= 1.0)

            onClicked: {
                verifySignTX.txSignRequest = bsApp.createTXSignRequest(
                            sendWalletsComboBox.currentIndex, recvAddress.text,
                            parseFloat(amount.text), parseFloat(fees.text),
                            txComment.text)
                stack.push(verifySignTX)
            }
        }
    }
}
