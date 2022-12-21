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
    property var searchStack: []
    property var searchHist: []
    id: explorer

    ExplorerEmpty {
        id: explorerEmpty
        visible: false
    }
    ExplorerAddress {
        id: explorerAddress
        visible: false
    }
    ExplorerTX {
        id: explorerTX
        visible: false
    }
//    ExplorerLoader {
//        id: explorerTX
//    }

    Column {
        spacing: 32
        anchors.fill: parent

        Row {
            id: textInput
            TextInput {
                id: expSearchBox
                width: 900
                height: 32
                color: 'lightgrey'
                font.pointSize: 14

                function textEntered() {
                    var rc = bsApp.startSearch(expSearchBox.text)
                    searchStack.push(expSearchBox.text)
                    if (rc === 0) {
                        ibFailure.displayMessage(qsTr("Unknown type of search key"))
                    }
                    else if (rc === 1) {    // address entered
                        explorerAddress.address = expSearchBox.text
                        explorerStack.push(explorerAddress)
                    }
                    else if (rc === 2) {    // TXid entered
                        explorerTX.tx = bsApp.getTXDetails(expSearchBox.text)
                        explorerStack.push(explorerTX)
                    }
                }

                onAccepted: textEntered()

                Text {
                    text: qsTr("Search for transaction or address")
                    color: 'darkgrey'
                    visible: !expSearchBox.text && !expSearchBox.activeFocus
                }
            }
            Button {
                text: qsTr("<")
                font.pointSize: 14
                width: 50
                enabled: (explorerStack.depth > 1)
                onClicked: {
                    explorerStack.pop()
                    searchHist.push(searchStack[searchStack.length - 1])
                    searchStack.pop
                    if (searchStack.length > 0) {
                        expSearchBox.text = searchStack[searchStack.length - 1]
                    }
                    expSearchBox.text = ""
                }
            }
            Button {
                text: qsTr(">")
                font.pointSize: 14
                width: 50
                enabled: true   //(searchHist.length > 0)
                onClicked: {
                    expSearchBox.text = searchHist[searchHist.length - 1]
                    expSearchBox.textEntered()
                    searchHist.pop
                }
            }
        }

        Item {
            Rectangle {
                anchors.top: parent.top + 100
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                //clip: true
                StackView {
                    id: explorerStack
                    anchors.fill: parent
                    initialItem: explorerEmpty
                    //anchors.top: parent.top + 100
                    //anchors.bottom: parent.bottom
                    //anchors.left: parent.left
                    //anchors.right: parent.right
                    //clip: true
                }
            }
        }
    }
}
