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

import "../BsStyles"

Button {
    id: control

    property color background_color: BSStyle.buttonsDisabledColor
    property int background_radius: 14

    width: 136
    height: 36
    hoverEnabled: true
    focusPolicy: Qt.NoFocus

    font.pixelSize: 12
    font.family: "Roboto"
    font.weight: Font.Normal
    font.letterSpacing: 0.3

    background: Rectangle {
        color: control.background_color
        radius: control.background_radius


        border.color: 
            (control.hovered ? BSStyle.comboBoxHoveredBorderColor :
            (control.activeFocus ? BSStyle.comboBoxFocusedBorderColor : BSStyle.comboBoxBorderColor))
        border.width: 1
    }

    contentItem: Text {
        text: control.text
        font: control.font
        color: BSStyle.textColor
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
