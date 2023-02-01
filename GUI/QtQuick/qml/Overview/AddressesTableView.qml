
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

import "../BsStyles"

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

    property int text_header_size: 10
    property int cell_text_size: 11

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
        color: row === 0 ? BSStyle.tableCellBackgroundColor : (row === selected_row_index ? BSStyle.tableCellSelectedBackgroundColor : BSStyle.tableCellBackgroundColor)

        Row {
            anchors.fill: parent

            Text {
                id: internal_text
                text: tableData

                color: row === 0 ? BSStyle.titleTextColor : BSStyle.textColor
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
                        text: qsTr("Copied")

                        font.pixelSize: 10
                        font.family: "Roboto"
                        font.weight: Font.Normal

                        contentItem: Text {
                            text: tool_tip.text
                            font: tool_tip.font
                            color: BSStyle.textColor
                        }

                        background: Rectangle {
                            color: BSStyle.buttonsStandardColor
                            border.color: BSStyle.buttonsStandardColor
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
            color: BSStyle.tableSeparatorColor

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
