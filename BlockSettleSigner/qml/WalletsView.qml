/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.9
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4
import QtQml.Models 2.3
import QtQuick.Window 2.3

import com.blocksettle.WalletsViewModel 1.0

import "StyledControls"
import "BsStyles"


TreeView {
    model: walletsModel
    objectName: "walletsView"

    selectionMode: SelectionMode.SingleSelection
    selection: treeViewSelectionModel

    ItemSelectionModel {
        id: treeViewSelectionModel
        model: walletsModel
    }

    TableViewColumn {
        id: columnName
        title: qsTr("Name")
        role: "name"
        width: 150
    }
    TableViewColumn {
        id: columnId
        title: qsTr("ID")
        role: "walletId"
        width: 100
    }
    TableViewColumn {
        id: columnType
        title: qsTr("Type")
        role: "walletType"
        width: 100
    }
    TableViewColumn {
        id: columnEncryption
        title: qsTr("Encryption")
        role: "state"
        width: 100
    }
    TableViewColumn {
        id: columnDescription
        title: qsTr("Description")
        role: "desc"
        width: parent.width - columnName.width - columnId.width - columnType.width - columnEncryption.width - 5
    }

    style: TreeViewStyle {
        backgroundColor: BSStyle.backgroundColor
        alternateBackgroundColor: "#32000000"
        textColor: BSStyle.textColor
        frame: Item {}
        headerDelegate: Rectangle {
                height: Math.round(textItem.implicitHeight * 1.2)
                width: textItem.implicitWidth
                color: "transparent"
                Text {
                    id: textItem
                    anchors.fill: parent
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: styleData.textAlignment
                    anchors.leftMargin: horizontalAlignment === Text.AlignLeft ? 12 : 1
                    anchors.rightMargin: horizontalAlignment === Text.AlignRight ? 8 : 1
                    text: styleData.value
                    elide: Text.ElideRight
                    color: textColor
                }
                Rectangle {
                    width: 1
                    height: parent.height - 2
                    y: 1
                    color: "#777"
                }
        }
    }

    function expandAll() {
        for (var i = 0; i < model.rowCount(); i++) {
            var index = model.index(i, 0)
            if (!isExpanded(index)) {
                expand(index)
            }
            for (var j = 0; j < model.rowCount(index); j++) {
                var child = model.index(j, 0, index)
                if (!isExpanded(child)) {
                    expand(child)
                }
            }
        }
    }

    onExpanded: {
        selection.setCurrentIndex(index, ItemSelectionModel.SelectCurrent)
    }

    onCollapsed: {
        selection.setCurrentIndex(index, ItemSelectionModel.SelectCurrent)
    }
}
