/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.15
import QtQuick.Controls 2.3

import "../BsStyles"

CustomTableView {
    id: component

    FontMetrics {
        id: fontMetrics
        font.pixelSize: BSSizes.applyScale(13)
        font.family: "Roboto"
        font.weight: Font.Normal
    }

    delegate: Rectangle {
        implicitHeight: BSSizes.applyScale(58)
        color: (row === component.selected_row_index ? BSStyle.tableCellSelectedBackgroundColor : BSStyle.tableCellBackgroundColor)

        MouseArea {
            anchors.fill: parent
            preventStealing: true
            propagateComposedEvents: true
            hoverEnabled: true

            onEntered: component.selected_row_index = row
            onExited: component.selected_row_index = -1
            onClicked: component.cellClicked(row, column, tableData, mouse)
            onDoubleClicked: component.cellDoubleClicked(row, column, tableData, mouse)
        }

        Item {
            width: parent.width
            height: parent.height

            Text {
                id: internal_text
                visible: column !== 1
                text: tableData
                height: parent.height
                verticalAlignment: Text.AlignTop
                clip: true

                color: dataColor
                font.family: "Roboto"
                font.weight: Font.Normal
                font.pixelSize: BSSizes.applyScale(13)

                leftPadding: BSSizes.applyScale(10)
                topPadding: BSSizes.applyScale(9)
            }

            Column {
                spacing: BSSizes.applyScale(8)
                visible: column === 1
                width: parent.width
                anchors.centerIn: parent

                Row {
                    width: parent.width

                    Text {
                        id: address_label_item
                        text: qsTr("Ad.:")
                        color: BSStyle.titleTextColor
                        font.family: "Roboto"
                        font.weight: Font.Normal
                        font.pixelSize: BSSizes.applyScale(13)
                        font.letterSpacing: -0.2
                        leftPadding: BSSizes.applyScale(10)
                    }
                    Text {
                        text: tableData
                        width: parent.width - fontMetrics.advanceWidth(address_label_item.text) - BSSizes.applyScale(10)
                        color: BSStyle.textColor
                        font.family: "Roboto"
                        font.weight: Font.Normal
                        font.pixelSize: BSSizes.applyScale(13)
                        font.letterSpacing: -0.2
                        clip: true
                    }
                }
                Row {
                    width: parent.width

                    Text {
                        id: transaction_label_item
                        text: qsTr("Tx.:")
                        color: BSStyle.titleTextColor
                        font.family: "Roboto"
                        font.weight: Font.Normal
                        font.pixelSize: BSSizes.applyScale(13)
                        font.letterSpacing: -0.2
                        leftPadding: BSSizes.applyScale(10)
                    }
                    Text {
                        text: txHash
                        width: parent.width - fontMetrics.advanceWidth(transaction_label_item.text) - BSSizes.applyScale(10)
                        color: BSStyle.textColor
                        font.family: "Roboto"
                        font.weight: Font.Normal
                        font.pixelSize: BSSizes.applyScale(13)
                        font.letterSpacing: -0.2
                        clip: true
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
    }
}
