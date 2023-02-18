
/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.15
import QtQuick.Controls 2.15

import "../BsStyles"
import "."

Column {
    id: root
    width: 1200
    height: 200
    spacing: 2

    property alias model: component.model
    property alias delegate: component.delegate
    property var columnWidths
    property int copy_button_column_index: 0

    signal copyRequested(var id)
    signal cellClicked(var row, var column, var data)
    signal cellDoubleClicked(var row, var column, var data)

    CustomHorizontalHeaderView {
        id: tableHeader
        width: parent.width
        syncView: component
        height: 32
    }

    TableView {
        id: component
        width: parent.width
        height: parent.height - tableHeader.height
        columnSpacing: 0
        rowSpacing: 0
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar { }


        columnWidthProvider: function (column) {
            return root.columnWidths[column] * component.width
        }

        property int selected_row_index: -1

        onWidthChanged: component.forceLayout()

        delegate: Rectangle {
            implicitHeight: 34
            color: row == component.selected_row_index ? BSStyle.tableCellSelectedBackgroundColor : BSStyle.tableCellBackgroundColor

            MouseArea {
                anchors.fill: parent
                preventStealing: true
                propagateComposedEvents: true
                hoverEnabled: true

                onEntered: component.selected_row_index = row
                onExited: component.selected_row_index = -1
                onClicked: root.cellClicked(row, column, tableData)
                onDoubleClicked: root.cellDoubleClicked(row, column, tableData)
            }

            Row {
                width: parent.width
                height: parent.height

                Text {
                    id: internal_text
                    text: tableData
                    height: parent.height
                    verticalAlignment: Text.AlignVCenter
                    clip: true

                    color: dataColor
                    font.family: "Roboto"
                    font.weight: Font.Normal
                    font.pixelSize: 12

                    leftPadding: 10
                }

                CopyIconButton {
                    id: copy_icon
                    x: internal_text.contentWidth + copy_icon.width / 2
                    visible: column === root.copy_button_column_index && row == component.selected_row_index
                    onCopy: root.copyRequested(tableData)
                }
            }

            Rectangle {
                height: 1
                width: parent.width
                color: BSStyle.tableSeparatorColor

                anchors.bottom: parent.bottom
            }
        }
    }
}
