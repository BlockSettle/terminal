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

    id: layout

    height: 662
    width: 1132
    spacing: 0

    CustomTitleLabel {
        id: title
        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : title.height
        text: "Select inputs"
    }

    Rectangle {
        id: addresses_rect

        Layout.topMargin: 24
        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter

        width: 1084
        height: 434
        color: "transparent"

        radius: 16

        border.color: "#3C435A"
        border.width: 1

        CustomTableView {

            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.left : parent.left
            anchors.rightMargin: 16
            anchors.right: parent.right

            model: txInputsModel
            columnWidths: [0.2, 0.2, 0.2, 0.2, 0.2]

            text_header_size: 12
            cell_text_size: 13
            copy_button_column_index: -1
            left_text_padding: 9
        }
    }

    Label {
        Layout.fillWidth: true
        Layout.fillHeight: true
    }
}
