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

    leftPadding: BSSizes.applyScale(6)
    topPadding: BSSizes.applyScale(4)
    bottomPadding: BSSizes.applyScale(4)
    rightPadding: BSSizes.applyScale(6)

    delegate: MenuItem {
        id: menuItem
        visible: menuItem.enabled
        width: BSSizes.applyScale(200)
        height: menuItem.visible ? BSSizes.applyScale(40) : 0 

        contentItem: Text {
            leftPadding: menuItem.indicator.width
            rightPadding: menuItem.arrow.width

            text: menuItem.text

            font.pixelSize: BSSizes.applyScale(12)
            font.family: "Roboto"
            font.weight: Font.Normal

            color: BSStyle.wildBlueColor

            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            width: parent.width - menu.leftPadding - menu.rightPadding
            height: parent.height

            radius: BSSizes.applyScale(14)
            color: menuItem.highlighted ? BSStyle.menuItemHoveredColor : BSStyle.menuItemColor
        }
    }


    background: Rectangle {
        implicitWidth: BSSizes.applyScale(200)
        implicitHeight: BSSizes.applyScale(40)
        color: BSStyle.popupBackgroundColor
        opacity: 1
        radius: BSSizes.applyScale(14)
        border.color : BSStyle.defaultBorderColor
        border.width : BSSizes.applyScale(1)
    }
}
