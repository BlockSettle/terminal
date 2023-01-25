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
import wallet.balance 1.0

Item {
    id: send

    VerifyTX {
        id: verifySignTX
        visible: false
    }
    SendAdvanced {
        id: advancedCreateTX
        visible: false
    }

    Column {
        spacing: 23
        anchors.fill: parent

        Row {
            Button {
                icon.source: "qrc:/images/send_icon.png"
                onClicked: {
                    stack.pop()
                }
            }
            Text {
                text: qsTr("<font color=\"white\">Send Bitcoin</font>")
                font.pointSize: 14
            }
            Button {
                width: 300
                text: qsTr("Advanced")
                font.pointSize: 14
                onClicked: {
                    bsApp.getUTXOsForWallet(sendWalletsComboBox.currentIndex)
                    txOutputsModel.clearOutputs()
                    txInputsModel.fee = fees.text
                    advancedCreateTX.walletIdx = sendWalletsComboBox.currentIndex
                    var outAmount = parseFloat(amount.text)
                    if ((outAmount >= 0.0000001) && (recvAddress.text.length)) {
                        txOutputsModel.addOutput(recvAddress.text, outAmount)
                        txInputsModel.getSelection()
                        advancedCreateTX.recvAddress = recvAddress.text
                        advancedCreateTX.sendAmount = amount.text
                    }
                    advancedCreateTX.comment = txComment.text
                    stack.push(advancedCreateTX)
                }
            }
        }

        TextInput {
            id: recvAddress
            width: 500
            height: 32
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
                width: 500
                height: 32
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
        Row {
            spacing: 23
            ComboBox {
                id: sendWalletsComboBox
                model: walletBalances
                textRole: "name"
                valueRole: "name"
                currentIndex: walletsComboBox.currentIndex
                font.pointSize: 14
                enabled: (bsApp.walletsList.length > 1)
                width: 350
            }
            Label {
                text: qsTr("<font color=\"white\">%1 BTC</font>").arg(walletBalances.data(
                    walletBalances.index(sendWalletsComboBox, 0), WalletBalance.TotalRole))
                font.pointSize: 14
            }
            ComboBox {
                id: fees
                model: feeSuggestions
                font.pointSize: 14
                textRole: "text"
                valueRole: "value"
                width: 500
                editable: true
            }
/*            TextInput {
                id: fees_
                width: 500
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
            }*/
        }
        TextEdit {
            id: txComment
            width: 900
            height: 84
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
                /*&& (parseFloat(fees.text) >= 1.0)*/

            onClicked: {
                console.log("fees.text = " + parseFloat(fees.text))
                console.log("amount.text = " + parseFloat(amount.text))
                console.log("recvAddress.text.length = " + recvAddress.text.length)
                verifySignTX.txSignRequest = bsApp.createTXSignRequest(
                            sendWalletsComboBox.currentIndex, recvAddress.text,
                            parseFloat(amount.text), /*parseFloat(fees.text)*/1.0,
                            txComment.text)
                stack.push(verifySignTX)
            }
        }
    }
}
