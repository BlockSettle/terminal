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
    id: transactions

    Column {
        spacing: 23
        anchors.fill: parent

        Row {
            spacing: 15

            Label {
                text: qsTr("<font color=\"white\">Transactions list (%1)</font>").arg(bsApp.nbTransactions)
                font.pointSize: 14
            }
            Item {  // spacer item
                Layout.fillWidth: true
                Layout.fillHeight: true
                Rectangle { anchors.fill: parent; color: "#ffaaaa" }
            }
            ComboBox {
                id: txWalletsComboBox
                objectName: "txWalletsComboBox"
                model: bsApp.txWalletsList
                font.pointSize: 8
            }
            ComboBox {
                id: txTypesComboBox
                objectName: "txTypesComboBox"
                model: bsApp.txTypesList
                font.pointSize: 8
            }
            Button {
                text: qsTr("From")
                font.pointSize: 8
            }
            Label {
                text: qsTr("<font color=\"darkgrey\">-</font>")
                font.pointSize: 12
            }
            Button {
                text: qsTr("To")
                font.pointSize: 8
            }
            TextEdit {
                id: txSearchBox
                width: 75
                height: 32
                Text {
                    text: qsTr("Search")
                    color: 'darkgrey'
                    visible: !txSearchBox.text && !txSearchBox.activeFocus
                }
            }
            Button {
                text: qsTr("CSV download")
                font.pointSize: 8
            }
        }

        TableView {
            width: 1000
            height: 600
            columnSpacing: 1
            rowSpacing: 1
            clip: true
            ScrollIndicator.horizontal: ScrollIndicator { }
            ScrollIndicator.vertical: ScrollIndicator { }
            model: txListModel
            delegate: Rectangle {
                implicitWidth: 125 * colWidth
                implicitHeight: 20
                border.color: "black"
                border.width: 1
                clip: true
                color: heading ? 'black' : 'darkslategrey'
                Text {
                    text: tableData
                    font.pointSize: heading ? 8 : 10
                    color: dataColor
                    anchors.centerIn: parent
                }
            }
        }
    }
}
