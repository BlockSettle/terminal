/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.15
import QtQuick.Controls 2.3

import "../BsStyles"

CustomSmallButton {
    id: control

    property alias custom_icon: icon_item

    Image {
        id: icon_item
        width: 10
        height: 10
        anchors.left: parent.left
        anchors.leftMargin: 14
        anchors.verticalCenter: parent.verticalCenter
    }

    contentItem: Text {
        text: control.text
        font: control.font
        color: BSStyle.titleTextColor
        verticalAlignment: Text.AlignVCenter

        leftPadding: icon_item.width + 10
    }
}