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
        title: qsTr("Name")
        role: "name"
        width: parent.width / 3.5
    }
    TableViewColumn {
        title: qsTr("Description")
        role: "desc"
        width: parent.width / 4
    }
    TableViewColumn {
        title: qsTr("ID")
        role: "walletId"
        width: parent.width / 6
    }
    TableViewColumn {
        title: qsTr("Type")
        role: "walletType"
        width: parent.width / 6
    }
    TableViewColumn {
        title: qsTr("Encryption Method")
        role: "state"
        width: parent.width / 5
    }

    style: TreeViewStyle {
        backgroundColor: BSStyle.backgroundColor
        alternateBackgroundColor: "#32000000"
        textColor: BSStyle.textColor
        frame: Item {}
        headerDelegate: Rectangle {
            height: textItem.implicitHeight * 1.2
            width: textItem.implicitWidth
            color: "transparent"
            Text {
                id: textItem
                anchors.fill: parent
                anchors.leftMargin: 12
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: styleData.textAlignment
                text: styleData.value
                color: textColor
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
}
