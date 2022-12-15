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
    property var addressInfo

    Column {
        spacing: 23
        anchors.fill: parent

        GridLayout {
            columns: 3
            Button {
                icon.source: "qrc:/images/send_icon.png"
                onClicked: {
                    stack.pop()
                }
            }
            Label {
                text: qsTr("<font color=\"darkgrey\">Transactions</font>")
                font.pointSize: 12
            }
            Label {
                text: qsTr("<font color=\"lightgrey\">1</font>")
                font.pointSize: 12
            }
            Image {
                source: "qrc:/images/bs_logo.png"
                Layout.rowSpan: 5
            }
            Label {
                text: qsTr("<font color=\"darkgrey\">Wallet</font>")
                font.pointSize: 12
            }
            Label {
                text: qsTr("<font color=\"lightgrey\">wallet</font>")
                font.pointSize: 12
            }
            Label {
                text: qsTr("<font color=\"darkgrey\">Address</font>")
                font.pointSize: 12
            }
            Row {
                Label {
                    id: address
                    text: qsTr("address")
                    color: 'lightgrey'
                    font.pointSize: 12
                }
                Button {
                    text: qsTr("Copy")
                    font.pointSize: 12
                    onClicked: {
                        bsApp.copyAddressToClipboard(address.text)
                    }
                }
            }
            Label {
                text: qsTr("<font color=\"darkgrey\">Address Type/ID</font>")
                font.pointSize: 12
            }
            Label {
                text: qsTr("<font color=\"lightgrey\">External/0</font>")
                font.pointSize: 12
            }
            Label {
                text: qsTr("<font color=\"darkgrey\">Comment</font>")
                font.pointSize: 12
            }
            Label {
                text: qsTr("<font color=\"lightgrey\">-</font>")
                font.pointSize: 12
            }
            Label {
                text: qsTr("<font color=\"darkgrey\">Balance</font>")
                font.pointSize: 12
            }
            Label {
                text: qsTr("<font color=\"lightgrey\">0</font>")
                font.pointSize: 12
            }
        }
        Row {
            Label {
                text: qsTr("<font color=\"lightgrey\">Incoming transactions</font>")
                font.pointSize: 14
            }
            Image {
                source: "qrc:/images/receive_icon.png"
            }
        }
        Row {
            Label {
                text: qsTr("<font color=\"lightgrey\">Outgoing transactions</font>")
                font.pointSize: 14
            }
            Image {
                source: "qrc:/images/send_icon.png"
            }
        }
    }
}
