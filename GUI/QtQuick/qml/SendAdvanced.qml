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
    property var recvAddress: ""
    property int walletIdx
    property var sendAmount: ""
    property var comment: ""
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
                        model: bsApp.walletsList
                        currentIndex: walletIdx
                        font.pointSize: 12
                        enabled: (bsApp.walletsList.length > 1)
                        width: 350
                        onCurrentIndexChanged: {
                            bsApp.getUTXOsForWallet(sendWalletsComboBox.currentIndex)
                        }
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
                    text: txInputsModel.fee
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
                    onAccepted: {
                        txInputsModel.fee = text
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
                        text: qsTr("<font color=\"darkgrey\">#Tx: %1</font>").arg(txInputsModel.nbTx)
                        font.pointSize: 10
                    }
                    Label {
                        text: qsTr("<font color=\"darkgrey\">Balance (BTC): %1</font>").arg(txInputsModel.balance)
                        font.pointSize: 10
                    }
                }
                TableView {
                    width: 500
                    height: 250
                    columnSpacing: 1
                    rowSpacing: 1
                    visible: cbSelectInputs.checked
                    clip: true
                    ScrollIndicator.horizontal: ScrollIndicator { }
                    ScrollIndicator.vertical: ScrollIndicator { }
                    model: txInputsModel
                    delegate: Rectangle {
                        implicitWidth: 100 * colWidth
                        implicitHeight: 20
                        border.color: "black"
                        border.width: 1
                        color: bgColor
                        Text {
                            text: tableData ? tableData : ""
                            font.pointSize: heading ? 8 : 10
                            color: dataColor
                            anchors.centerIn: parent
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                if (!heading) {
                                    if (model.column === 0) {
                                        txInputsModel.toggleSelection(model.row)
                                    }
                                    else if (model.column === 1) {
                                        txInputsModel.toggle(model.row)
                                    }
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
                    id: outAddr
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
                        id: amount
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
                    model: txOutputsModel
                    delegate: Rectangle {
                        implicitWidth: 160 * colWidth
                        implicitHeight: 20
                        border.color: "black"
                        border.width: 1
                        color: heading ? 'black' : 'darkslategrey'
                        Text {
                            text: tableData ? tableData : ""
                            font.pointSize: heading ? 8 : 10
                            color: dataColor
                            anchors.centerIn: parent
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                if (!heading && (model.column === 2)) {
                                    txOutputsModel.delOutput(model.row)
                                }
                            }
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
                            txComment.text, txInputsModel.getSelection())
                stack.push(verifySignTX)
            }
        }
    }
}
