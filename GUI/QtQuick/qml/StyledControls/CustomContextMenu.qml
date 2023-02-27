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

Menu {
    id: menu

    leftPadding: 6
    topPadding: 4
    bottomPadding: 4

    delegate: MenuItem {
        id: menuItem
        implicitWidth: 200
        implicitHeight: 40

        contentItem: Text {
            leftPadding: menuItem.indicator.width
            rightPadding: menuItem.arrow.width

            text: menuItem.text

            font.pixelSize: 12
            font.family: "Roboto"
            font.weight: Font.Normal

            color: BSStyle.wildBlueColor

            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            implicitWidth: 200
            implicitHeight: 40

            radius: 14
            color: menuItem.highlighted ? BSStyle.menuItemHoveredColor : BSStyle.menuItemColor
        }
    }


    background: Rectangle {
        implicitWidth: 200
        implicitHeight: 40
        color: BSStyle.popupBackgroundColor
        opacity: 1
        radius: 14
        border.color : BSStyle.defaultBorderColor
        border.width : 1
    }
}
