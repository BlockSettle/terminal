/*

***********************************************************************************
* Copyright (C) 2018 - 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/

import QtQuick 2.9
import QtQuick.Controls 2.3
import "../BsStyles"

Rectangle {
    id: rect

    //aliases
    property alias icon_source: icon.source
    property alias icon_visible: icon.visible
    //usually we dont need only if custom margin and size
    property alias _icon: icon
    property alias icon_add_source: icon_add.source
    property alias title_text: title.text

    signal clicked_add()
    signal clicked()

    width: 532
    height: 50

    color: "transparent"
    opacity: 1
    radius: 14

    border.color: mouseArea.containsMouse ? BSStyle.listItemHoveredBorderColor : BSStyle.listItemBorderColor
    border.width: 1

    Image {
        id: icon

        anchors.verticalCenter: parent.verticalCenter
        anchors.left: rect.left
        anchors.leftMargin: 16

        width: 24
        height: 24
        sourceSize.width: 24
        sourceSize.height: 24
    }

    Label {
        id: title

        anchors.verticalCenter: parent.verticalCenter
        anchors.left: icon.right
        anchors.leftMargin: 8

        horizontalAlignment : Text.AlignLeft

        font.pixelSize: 16
        font.family: "Roboto"
        font.weight: Font.Normal

        color: "#7A88B0"
    }

    Image {
        id: icon_add

        visible: source.toString().length > 0

        anchors.verticalCenter: parent.verticalCenter
        anchors.right: rect.right
        anchors.rightMargin: 13

        z: 1
        width: 24
        height: 24
        sourceSize.width: 24
        sourceSize.height: 24

        MouseArea {
            anchors.fill: parent
            onClicked: {
                rect.clicked_add()
            }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        //propagateComposedEvents: true
        onClicked: {
            rect.clicked()
            //mouse.accepted = false
        }
    }
}
