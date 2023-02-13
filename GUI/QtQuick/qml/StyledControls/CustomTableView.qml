
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

TableView {
    id: component
    width: 1200
    height: 200

    columnSpacing: 0
    rowSpacing: 0
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    ScrollBar.vertical: ScrollBar { }

    signal copyRequested(var id)
    signal deleteRequested(int id)
    signal cellClicked(int row, int column, var data)
    signal cellDoubleClicked(int row, int column, var data)

    property int text_header_size: 11    
    property int cell_text_size: 12
    property int copy_button_column_index: 0
    property int delete_button_column_index: -1

    property int left_first_header_padding: -1
    property int left_text_padding: 10

    property var columnWidths:  ({})
    columnWidthProvider: function (column) {
        return columnWidths[column] * component.width
    }

    property int selected_row_index: -1

    onWidthChanged: component.forceLayout()

    delegate: Rectangle {
        id: delega

        implicitHeight: 34
        color: row === 0 ? BSStyle.tableCellBackgroundColor : (row === selected_row_index ? BSStyle.tableCellSelectedBackgroundColor : BSStyle.tableCellBackgroundColor)

        MouseArea {
            anchors.fill: parent
            preventStealing: true
            propagateComposedEvents: true
            hoverEnabled: true

            onEntered: {
                if (row !== 0) {
                    component.selected_row_index = row
                }
            }

            onExited: {
                if (row !== 0) {
                    component.selected_row_index = -1
                }
            }

            onClicked: {
                if (row !== 0) {
                    component.cellClicked(row, column, tableData)
                }
            }
            onDoubleClicked: {
                if (row !== 0) {
                    omponent.cellDoubleClicked(row, column, tableData)
                }
            } 
        }

        Loader {
            id: row_loader

            width: parent.width
            height: childrenRect.height

            property int delete_button_column_index: component.delete_button_column_index
            property int text_header_size: component.text_header_size
            property int cell_text_size: component.cell_text_size
            property int copy_button_column_index: component.copy_button_column_index
            property int selected_row_index: component.selected_row_index
            property int left_first_header_padding: component.left_first_header_padding
            property int left_text_padding: component.left_text_padding
            property int model_row: row
            property int model_column: column

            property string model_tableData: tableData
            property var model_dataColor: dataColor

            Component.onCompleted: {
                delega.update_row_loader_size()
            }
        }

        Connections {
            target: row_loader.item
            function onDeleteRequested (row)
            {
                component.deleteRequested(row)
            }
            function onCopyRequested (tableData)
            {
                component.copyRequested(tableData)
            }
        }

        onWidthChanged: {
            delega.update_row_loader_size()
        }

        onHeightChanged: {
            delega.update_row_loader_size()
        }

        function update_row_loader_size()
        {
            row_loader.width = delega.width
            row_loader.height = childrenRect.height
            if (row_loader.width && row_loader.height && (row_loader.source !== ""))
            {
                row_loader.setSource(choose_row_source(row, column))
            }
        }

        Rectangle {
            height: 1
            width: parent.width
            color: BSStyle.tableSeparatorColor

            anchors.bottom: parent.bottom
        }
    }

    function choose_row_source(row, column)
    {
        return "CustomTableDelegateRow.qml"
    }
}
