/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.15
import QtQuick.Controls 2.3

Button {
    id: control

    property color background_color: "#32394F"
    property color background_border_color: "white"
    property int background_radius: 14
    property int background_border_size: 0

    text: "Wallet properties"

    width: 136
    height: 36

    font.pixelSize: 12
    font.family: "Roboto"
    font.weight: Font.Normal

    background: Rectangle {
        color: control.background_color
        radius: control.background_radius

        border.width: control.background_border_size
        border.color: control.background_border_color
    }

    contentItem: Text {
        text: control.text
        font: control.font
        color: "#FFFFFF"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
