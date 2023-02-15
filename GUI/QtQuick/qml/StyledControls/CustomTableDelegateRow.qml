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

Component
{
    id: cmpnt

    Row {
        id: root

        signal deleteRequested (int row)
        signal copyRequested (string tableData)

        Text {
            id: internal_text

            visible: model_column !== delete_button_column_index

            text: model_tableData
            height: parent.height
            verticalAlignment: Text.AlignVCenter
            clip: true

            color: get_data_color(model_row, model_column)
            font.family: "Roboto"
            font.weight: Font.Normal
            font.pixelSize: model_row === 0 ? text_header_size : cell_text_size

            leftPadding: get_text_left_padding(model_row, model_column, model_is_expandable)
        }

        DeleteIconButton {
            id: delete_icon
            x: 0
            visible: model_column === delete_button_column_index && model_row > 0
            onDeleteRequested: root.deleteRequested(model_row)
        }

        CopyIconButton {
            id: copy_icon
            x: internal_text.contentWidth + copy_icon.width / 2
            visible: model_column === copy_button_column_index && model_row === selected_row_index
            onCopy: root.copyRequested(model_tableData)
        }
    }
}
