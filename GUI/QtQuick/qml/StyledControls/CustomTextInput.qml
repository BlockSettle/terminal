/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.9
import QtQuick.Controls 2.3

import "../BsStyles"

TextField {
    id: root
    horizontalAlignment: Text.AlignHLeft
    font.pixelSize: 11
    color: BSStyle.inputsFontColor
    padding: 0
    selectByMouse: true

    background: Rectangle {
        implicitWidth: 200
        implicitHeight: 25
        color: "transparent"
        border.color: BSStyle.inputsBorderColor
    }

    CustomContextMenu {
        anchors.fill: parent
    }
}
