/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.15

Rectangle {
    id: control

    property string label_text: "Confirmed balance"
    property color label_text_color: "#7A88B0"
    property int label_text_font_size: 12

    property string label_value: "0.00999889"
    property color label_value_color: "#E2E7FF"
    property int label_value_font_size: 13

    property string value_suffix: "BTC"
    property int left_text_padding: 10

    width: 120
    height: 53
    color: "#191E2A"

    Column {
        anchors.verticalCenter: parent.verticalCenter
        spacing: 5

        Text {
            text: control.label_text
            leftPadding: control.left_text_padding

            color: control.label_text_color
            font.pixelSize: control.label_text_font_size
        }

        Text {
            text: control.label_value + " " + control.value_suffix
            leftPadding: control.left_text_padding

            color: control.label_value_color
            font.weight: Font.Medium
            font.pixelSize: control.label_value_font_size
        }
    }
}
