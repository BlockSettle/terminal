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

    width: 134
    height: 29

    focusPolicy: Qt.TabFocus
    font.pixelSize: 12
    font.family: "Roboto"
    font.letterSpacing: 0.3

    hoverEnabled: true

    property alias backgroundColor: backgroundItem.color

    background: Rectangle {
        id: backgroundItem
        color: "#020817"
        radius: 14

        border.color: 
            (control.hovered ? BSStyle.comboBoxHoveredBorderColor :
            (control.activeFocus ? BSStyle.comboBoxFocusedBorderColor : BSStyle.comboBoxBorderColor))
        border.width: 1
    }

    contentItem: Text {
        text: control.text
        font: control.font
        anchors.fill: parent
        color: BSStyle.titleTextColor
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
    }
}
