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

    width: 150
    height: 50

    font.pixelSize: 14
    font.family: "Roboto"
    font.letterSpacing: 0.5

    hoverEnabled: true
    activeFocusOnTab: true

    background: Rectangle {
        id: backgroundItem
        color: control.enabled ? "#f05c44" : "black"
        radius: 4

        border.color: 
            (enabled ? "transparent" :
            (control.hovered ? "white" :
            (control.activeFocus ? "gray" : "gray")))
        border.width: 1
    }

    contentItem: Text {
        text: control.text
        font: control.font
        anchors.fill: parent
        color: control.enabled ? "black" : "white"
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
    }
}
