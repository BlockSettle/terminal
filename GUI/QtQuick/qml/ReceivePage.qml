/*

***********************************************************************************
* Copyright (C) 2018 - 2022, BlockSettle AB
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
    id: receive

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
            text: qsTr("<font color=\"white\">Generate address</font>")
            font.pointSize: 14
        }
        Row {
            spacing: 23
            ComboBox {
                id: receiveWalletsComboBox
                model: bsApp.walletsList
                currentIndex: walletsComboBox.currentIndex
                font.pointSize: 14
                width: 500
                visible: (bsApp.walletsList.length > 1)

                onActivated: (index) => {
                    bsApp.walletSelected(index)
                }
            }
            Label {
                text: qsTr("<font color=\"white\">%1 BTC</font>").arg(bsApp.totalBalance)
                font.pointSize: 14
            }
        }
        Label {
            text: bsApp.generatedAddress.length ? qsTr("Bitcoins sent to this address will appear in %1").arg(receiveWalletsComboBox.textAt(receiveWalletsComboBox.currentIndex))
                                                : qsTr("<font color=\"lightgrey\">Select address type</font>")
            font.pointSize: 12
        }
        Row {
            spacing: 5
            visible: !bsApp.generatedAddress.length

            RadioButton {
                id: rbNative
                checked: true
                text: qsTr("<center><font color=\"white\">Native SegWit</font><br><cont color=\"darkgrey\">(bech32)</font></center>")
                font.pointSize: 10
            }
            RadioButton {
                id: rbNested
                checked: false
                text: qsTr("<center><font color=\"white\">Nested SegWit</font><br><cont color=\"darkgrey\">(P2SH)</font></center>")
                font.pointSize: 10
            }
        }
        Label {
            visible: bsApp.generatedAddress.length
            width: 900
            height: 32
            text: qsTr("<font color=\"white\">%1</font>").arg(bsApp.generatedAddress)
            font.pointSize: 14
        }
        Button {
            visible: !bsApp.generatedAddress.length
            width: 600
            text: qsTr("Generate")
            font.pointSize: 14
            onClicked: {
                bsApp.generateNewAddress(receiveWalletsComboBox.currentIndex, rbNative.checked)
            }
        }
        Button {
            visible: bsApp.generatedAddress.length
            width: 600
            text: qsTr("Copy to clipboard")
            font.pointSize: 14
            onClicked: {
                bsApp.copyAddressToClipboard(bsApp.generatedAddress)
            }
        }

        TableView {
            width: 1000
            height: 300
            columnSpacing: 1
            rowSpacing: 1
            clip: true
            ScrollIndicator.horizontal: ScrollIndicator { }
            ScrollIndicator.vertical: ScrollIndicator { }
            model: addressListModel
            delegate: Rectangle {
                implicitWidth: firstcol ? 550 : 150
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
