
/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.15
import QtQuick.Controls 2.15

TableView {
    id: component
    width: 1200
    height: 200

    columnSpacing: 0
    rowSpacing: 0
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    signal copyRequested(var id)

    property bool has_copy: true

    property color text_header_color: "#7A88B0"
    property int text_header_size: 10

    property color cell_text_color: "#FFFFFF"
    property int cell_text_size: 11

    property color separator_color: "#3C435A"
    property color cell_background_color: "#333C435A"
    property color selected_cell_background_color: "#22293B"
    property color header_background_color: "#333C435A"

    property var columnWidths: [350, 100, 120, 100]
    columnWidthProvider: function (column) {
        return (column === (columnWidths.length
                            - 1)) ? Math.max(
                                        columnWidths[column],
                                        component.width - columnWidths.reduce(
                                            (sum, a) => sum + a,
                                            0) + columnWidths.slice(
                                            -1)[0]) : columnWidths[column]
    }

    property int selected_row_index: -1

    delegate: Rectangle {
        implicitHeight: 34
        color: row === 0 ? component.header_background_color : (row === selected_row_index ? component.selected_cell_background_color : component.cell_background_color)

        Row {
            anchors.fill: parent

            Text {
                id: internal_text
                text: tableData

                color: row === 0 ? component.text_header_color : component.cell_text_color
                font.family: "Roboto"
                font.weight: Font.Normal
                font.pixelSize: row === 0 ? component.text_header_size : component.cell_text_size

                leftPadding: 10

                anchors.verticalCenter: parent.verticalCenter
            }

            Image {
                width: 24
                height: 24
                visible: column === 0 && row == selected_row_index && component.has_copy
                anchors.verticalCenter: parent.verticalCenter
                source: "qrc:/images/overview/copy_button.svg"

                MouseArea {
                    anchors.fill: parent

                    ToolTip {
                        id: tool_tip
                        timeout: 1000
                        text: "Copied"

                        font.pixelSize: 10
                        font.family: "Roboto"
                        font.weight: Font.Normal

                        contentItem: Text {
                            text: tool_tip.text
                            font: tool_tip.font
                            color: "#FFFFFF"
                        }

                        background: Rectangle {
                            color: "#191E2A"
                            border.color: "#3C435A"
                            border.width: 1
                            radius: 14
                        }
                    }

                    onClicked: {
                        component.copyRequested(tableData)
                        tool_tip.visible = true
                    }
                }
            }
        }

        Rectangle {
            height: 1
            width: parent.width
            color: component.separator_color

            anchors.bottom: parent.bottom
        }

        MouseArea {
            anchors.fill: parent
            preventStealing: true
            propagateComposedEvents: true
            hoverEnabled: true

            onClicked: mouse.accepted = false

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
        }
    }
}
