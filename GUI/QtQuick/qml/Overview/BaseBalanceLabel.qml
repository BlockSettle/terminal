/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.15

import "../BsStyles"

Rectangle {
    id: control

    property string label_text
    property int label_text_font_size: 12

    property string label_value
    property int label_value_font_size: 13

    property string value_suffix
    property int left_text_padding: 10

    width: 120
    height: 53
    color: "transparent"

    Column {
        anchors.verticalCenter: parent.verticalCenter
        spacing: 5

        Text {
            text: control.label_text
            leftPadding: control.left_text_padding

            color: BSStyle.titleTextColor
            font.pixelSize: control.label_text_font_size
        }

        Text {
            text: control.label_value + " " + control.value_suffix
            leftPadding: control.left_text_padding

            color: BSStyle.balanceValueTextColor
            font.weight: Font.Medium
            font.pixelSize: control.label_value_font_size
        }
    }
}
