/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"

ColumnLayout  {
    property var inputsModel: null
    id: layout

    property int text_header_size: BSSizes.applyScale(11)
    property int cell_text_size: BSSizes.applyScale(12)

    height: BSSizes.applyScale(662)
    width: BSSizes.applyScale(1132)
    spacing: 0

    CustomTitleLabel {
        id: title
        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : title.height
        text: qsTr("Select inputs")
    }

    Rectangle {
        id: addresses_rect

        Layout.topMargin: BSSizes.applyScale(24)
        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter

        width: BSSizes.applyScale(1084)
        height: BSSizes.applyScale(434)
        color: "transparent"

        radius: BSSizes.applyScale(16)

        border.color: BSStyle.defaultBorderColor
        border.width: 1

        CustomTableView {
            id: inputs_table

            width: parent.width - BSSizes.applyScale(32)
            height: parent.height - BSSizes.applyScale(12)
            anchors.centerIn: parent

            model: inputsModel
            columnWidths: [0.7, 0.1, 0.1, 0.1]

            copy_button_column_index: -1
            has_header: false

            Component
            {
                id: cmpnt_address_item

                Item {
                    id: address_item

                    CustomCheckBox {
                        id: checkbox_address

                        anchors.left: parent.left
                        anchors.leftMargin: inputs_table.get_text_left_padding(model_row, model_column, model_is_expandable)
                        anchors.verticalCenter: parent.verticalCenter

                        checked: model_selected
                        checkable: true

                        onClicked: {
                            inputsModel.toggleSelection(model_row)
                        }
                    }

                    Text {
                        id: internal_text

                        anchors.left: checkbox_address.right
                        anchors.leftMargin: BSSizes.applyScale(8)
                        anchors.verticalCenter: parent.verticalCenter

                        visible: model_column !== delete_button_column_index

                        text: model_tableData
                        height: parent.height
                        verticalAlignment: Text.AlignVCenter
                        clip: true

                        color: get_data_color(model_row, model_column)
                        font.family: "Roboto"
                        font.weight: Font.Normal
                        font.pixelSize: model_row === 0 ? text_header_size : cell_text_size

                        MouseArea {
                            anchors.fill: parent

                            onClicked: {
                                if (model_row !== 0)
                                    inputsModel.toggle(model_row)
                            }
                        }
                    }

                    Image {
                        id: arrow_icon

                        visible: model_is_expandable

                        anchors.left: internal_text.left
                        anchors.leftMargin: BSSizes.applyScale(10) + internal_text.contentWidth
                        anchors.verticalCenter: parent.verticalCenter

                        width: BSSizes.applyScale(9)
                        height: BSSizes.applyScale(6)
                        sourceSize.width: BSSizes.applyScale(9)
                        sourceSize.height: BSSizes.applyScale(6)

                        source: model_expanded? "qrc:/images/expanded.svg" : "qrc:/images/collapsed.svg"

                        MouseArea {
                            anchors.fill: parent

                            onClicked: {
                                if (model_row !== 0)
                                    inputsModel.toggle(model_row)
                            }
                        }
                    }
                }
            }

            CustomTableDelegateRow {
                id: cmpnt_table_delegate
            }

            function choose_row_source_component(row, column)
            {
                return (column === 0) ? cmpnt_address_item : cmpnt_table_delegate
            }

            function get_text_left_padding(row, column, isExpandable)
            {
                return (!isExpandable && column === 0 && row !== 0) ? 61 : left_text_padding
            }

            function get_line_left_padding(row, column, isExpandable)
            {
                return (!isExpandable && column === 0 && row !== 0) ? 51 : 0
            }
        }
    }

    Label {
        id: inputs_details_title

        Layout.leftMargin: BSSizes.applyScale(26)
        Layout.topMargin: BSSizes.applyScale(24)
        Layout.alignment: Qt.AlignLeft | Qt.AlingTop

        text: qsTr("Inputs details")

        height : BSSizes.applyScale(19)
        color: "#E2E7FF"
        font.pixelSize: BSSizes.applyScale(16)
        font.family: "Roboto"
        font.weight: Font.Medium
    }

    Rectangle {
        id: total_rect

        Layout.topMargin: BSSizes.applyScale(16)
        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter

        width: BSSizes.applyScale(1084)
        height: BSSizes.applyScale(82)
        color: "#32394F"

        radius: BSSizes.applyScale(14)

        Label {

            id: trans_inputs_title

            anchors.top: parent.top
            anchors.topMargin: BSSizes.applyScale(18)
            anchors.left: parent.left
            anchors.leftMargin: BSSizes.applyScale(20)

            text: qsTr("Transaction Inputs:")

            color: "#45A6FF"

            font.pixelSize: BSSizes.applyScale(14)
            font.family: "Roboto"
            font.weight: Font.Normal
        }

        Label {

            id: total_amount_title

            anchors.top: trans_inputs_title.bottom
            anchors.topMargin: BSSizes.applyScale(14)
            anchors.left: parent.left
            anchors.leftMargin: BSSizes.applyScale(20)

            text: qsTr("Total Amount:")

            color: "#45A6FF"

            font.pixelSize: BSSizes.applyScale(14)
            font.family: "Roboto"
            font.weight: Font.Normal
        }

        Label {

            id: trans_inputs

            anchors.top: parent.top
            anchors.topMargin: BSSizes.applyScale(18)
            anchors.right: parent.right
            anchors.rightMargin: BSSizes.applyScale(20)

            text: inputsModel.nbTx

            color: "#FFFFFF"

            font.pixelSize: BSSizes.applyScale(14)
            font.family: "Roboto"
            font.weight: Font.Normal
        }

        Label {

            id: total_amount

            anchors.top: trans_inputs.bottom
            anchors.topMargin: BSSizes.applyScale(18)
            anchors.right: parent.right
            anchors.rightMargin: BSSizes.applyScale(20)

            text: inputsModel.balance

            color: "#FFFFFF"

            font.pixelSize: BSSizes.applyScale(14)
            font.family: "Roboto"
            font.weight: Font.Normal
        }
    }

    Label {
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    function init()
    {
    }
}
