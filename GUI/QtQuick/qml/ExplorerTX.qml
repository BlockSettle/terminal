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
    property var txId

    Column {
        spacing: 23
        anchors.fill: parent

        Label {
            text: " "
            font.pointSize: 50
            height: 50
        }
        Row {
            spacing: 16
            Label {
                text: qsTr("<font color=\"white\">Transaction ID</font>")
                font.pointSize: 14
            }
            Label {
                text: txId
                color: 'lightgrey'
                font.pointSize: 12
            }
            Button {
                text: qsTr("Copy")
                font.pointSize: 12
                onClicked: {
                    bsApp.copyAddressToClipboard(txId)
                }
            }
        }
        Row {
            spacing: 12
            Column {
                Label {
                    text: qsTr("<font color=\"gray\">Confirmations</font>")
                    font.pointSize: 8
                }
                Label {
                    text: qsTr("<font color=\"green\">283</font>")
                    font.pointSize: 12
                }
            }
            Column {
                Label {
                    text: qsTr("<font color=\"gray\">Inputs</font>")
                    font.pointSize: 8
                }
                Label {
                    text: qsTr("<font color=\"white\">1</font>")
                    font.pointSize: 12
                }
            }
            Column {
                Label {
                    text: qsTr("<font color=\"gray\">Outputs</font>")
                    font.pointSize: 8
                }
                Label {
                    text: qsTr("<font color=\"white\">2</font>")
                    font.pointSize: 12
                }
            }
            Column {
                Label {
                    text: qsTr("<font color=\"gray\">Input Amount (BTC)</font>")
                    font.pointSize: 8
                }
                Label {
                    text: qsTr("<font color=\"white\">0.01959741</font>")
                    font.pointSize: 12
                }
            }
            Column {
                Label {
                    text: qsTr("<font color=\"gray\">Output Amount (BTC)</font>")
                    font.pointSize: 8
                }
                Label {
                    text: qsTr("<font color=\"white\">0.01959741</font>")
                    font.pointSize: 12
                }
            }
            Column {
                Label {
                    text: qsTr("<font color=\"gray\">Fees (BTC)</font>")
                    font.pointSize: 8
                }
                Label {
                    text: qsTr("<font color=\"white\">0.00000146</font>")
                    font.pointSize: 12
                }
            }
            Column {
                Label {
                    text: qsTr("<font color=\"gray\">Fee per byte (s/b)</font>")
                    font.pointSize: 8
                }
                Label {
                    text: qsTr("<font color=\"white\">1</font>")
                    font.pointSize: 12
                }
            }
            Column {
                Label {
                    text: qsTr("<font color=\"gray\">Size (virtual bytes)</font>")
                    font.pointSize: 8
                }
                Label {
                    text: qsTr("<font color=\"white\">141</font>")
                    font.pointSize: 12
                }
            }
        }
        Row {
            spacing: 32
            Column {
                Label {
                    text: qsTr("<font color=\"white\">Input</font>")
                    font.pointSize: 14
                }
                TableView {
                    width: 500
                    height: 300
                    columnSpacing: 1
                    rowSpacing: 1
                    clip: true
                    ScrollIndicator.horizontal: ScrollIndicator { }
                    ScrollIndicator.vertical: ScrollIndicator { }
                    model: addressListModel
                    delegate: Rectangle {
                        implicitWidth: 120
                        implicitHeight: 40
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
                                if (!heading && (model.column === 0)) {
                                    isTXSearch = false
                                    isAddressSearch = true
                                }
                            }
                        }
                    }
                }
            }
            Column {
                Label {
                    text: qsTr("<font color=\"white\">Output</font>")
                    font.pointSize: 14
                }
                TableView {
                    width: 500
                    height: 300
                    columnSpacing: 1
                    rowSpacing: 1
                    clip: true
                    ScrollIndicator.horizontal: ScrollIndicator { }
                    ScrollIndicator.vertical: ScrollIndicator { }
                    model: addressListModel
                    delegate: Rectangle {
                        implicitWidth: 120
                        implicitHeight: 40
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
                                if (!heading && (model.column === 0)) {
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
