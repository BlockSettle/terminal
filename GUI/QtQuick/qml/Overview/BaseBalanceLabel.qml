/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
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
    property string label_value
    property color label_value_color: BSStyle.balanceValueTextColor

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
            font.family: "Roboto"
            font.pixelSize: 12
            font.letterSpacing: -0.2
        }

        Text {
            text: control.label_value + " " + control.value_suffix
            leftPadding: control.left_text_padding

            color: control.label_value_color
            font.family: "Roboto"
            font.weight: Font.Bold
            font.pixelSize: 14
            font.letterSpacing: 0.2
        }
    }
}
