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
    
    property int historyIndex: -1
    property var searchHist: []
    
    id: explorer

    ExplorerEmpty {
        id: explorerEmpty
        visible: false
    }
    ExplorerAddress {
        id: explorerAddress
        visible: false
        onRequestPageChange: (text) => { expSearchBox.requestSearchText(text) }
    }
    ExplorerTX {
        id: explorerTX
        visible: false
        onRequestPageChange: (text) => { expSearchBox.requestSearchText(text) }
    }
//    ExplorerLoader {
//        id: explorerTX
//    }

    Column {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            id: explorer_menu_row
            height: 100
            width: parent.width

            color: "transparent"

            Rectangle {
                anchors.bottomMargin: 24
                anchors.topMargin: 24
                anchors.leftMargin: 18
                anchors.rightMargin: 18
                anchors.fill: parent
                anchors.centerIn: parent

                radius: 14
                color: "#020817"

                border.width: 1
                border.color: BSStyle.comboBoxBorderColor

                Row {
                    spacing: 20
                    anchors.fill: parent
                    anchors.leftMargin: 18

                    Image {
                        id: search_icon
                        width: 24
                        height: 24
                        source: "qrc:/images/search_icon.svg"
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    TextInput {
                        id: expSearchBox
                        anchors.verticalCenter: parent.verticalCenter
                        width: 900
                        clip: true
                        color: 'lightgrey'
                        font.pixelSize: 16

                        function resetSearch() {
                            searchHist = []
                            historyIndex = -1
                            expSearchBox.clear()
                            explorerStack.replace(explorerEmpty)
                        }

                        function prev() {
                            if (historyIndex >= 0) {
                                historyIndex--;
                                if (historyIndex >= 0) {
                                    expSearchBox.text = searchHist[historyIndex]
                                    openSearchResult()
                                }
                                else {
                                    expSearchBox.clear()
                                    explorerStack.replace(explorerEmpty)
                                }
                            }
                        }

                        function next() {
                            if (historyIndex < (searchHist.length - 1)) {
                                historyIndex++;
                                expSearchBox.text = searchHist[historyIndex]
                                openSearchResult()
                            }
                        }

                        function requestSearch() {
                            if (historyIndex >= 0)
                                searchHist = searchHist.slice(0, historyIndex + 1)
                            searchHist.push(expSearchBox.text)
                            historyIndex++;
                            openSearchResult()
                        }

                        function requestSearchText(newText) {
                            expSearchBox.text = newText
                            requestSearch()
                        }

                        function openSearchResult() {
                            var rc = bsApp.getSearchInputType(expSearchBox.text)
                            if (rc === 0) {
                                ibFailure.displayMessage(qsTr("Unknown type of search key"))
                            }
                            else if (rc === 1) {    // address entered
                                explorerAddress.address = expSearchBox.text
                                bsApp.startAddressSearch(explorerAddress.address)
                                explorerStack.replace(explorerAddress)
                            }
                            else if (rc === 2) {    // TXid entered
                                explorerTX.tx = bsApp.getTXDetails(expSearchBox.text)
                                explorerStack.replace(explorerTX)
                            }
                        }

                        onAccepted: requestSearch()

                        Text {
                            text: qsTr("Search for transaction or address")
                            font.pixelSize: 16
                            color: BSStyle.titleTextColor
                            anchors.fill: parent
                            visible: !expSearchBox.text && !expSearchBox.activeFocus
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }

                Row {
                    id: right_buttons_menu
                    spacing: 8
                    anchors.rightMargin: 11
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    
                    Image {
                        width: 24
                        height: 24
                        source: "qrc:/images/paste_icon.png"
                        anchors.verticalCenter: parent.verticalCenter

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                expSearchBox.clear()
                                expSearchBox.paste()
                                expSearchBox.requestSearch()
                            }
                        }
                    } 

                    CustomSmallButton {
                        text: qsTr("<")
                        width: 29
                        height: 29
                        enabled: historyIndex >= 0
                        onClicked: expSearchBox.prev()
                    }

                    CustomSmallButton {
                        text: qsTr(">")
                        width: 29
                        height: 29
                        enabled: historyIndex < (searchHist.length - 1)
                        onClicked: expSearchBox.next()
                    }

                    CustomSmallButton {
                        text: qsTr("Reset")
                        width: 68
                        height: 29
                        onClicked: expSearchBox.resetSearch()
                    }
                }
            }
        }

        StackView {
            id: explorerStack
            width: parent.width
            height: parent.height - explorer_menu_row.height
            initialItem: explorerEmpty
        }
    }
}
