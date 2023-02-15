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

Rectangle {
    id: root

    property bool isFullChoosed: true
    signal sig_full_changed (bool isFull)

    width: 530
    height: 40

    color: "transparent"
    radius: 37
    border.color : BsStyle.defaultBorderColor
    border.width : 1

    Rectangle {
        id: left_rect

        width: 260
        height: 34

        anchors.top: root.top
        anchors.topMargin: 3
        anchors.left: root.left
        anchors.leftMargin: 3

        color: isFullChoosed? "#32394F": "transparent"
        radius: 37

        Label {
            id: left_label

            width: 260
            height: 15

            anchors.centerIn  : left_rect
            horizontalAlignment :  Text.AlignHCenter

            text: "Full"

            color: isFullChoosed? "#E2E7FF": "#7A88B0"

            font.pixelSize: 13
            font.family: "Roboto"
            font.weight: Font.Medium
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                isFullChoosed = true
                sig_full_changed(isFullChoosed)
            }
        }
    }

    Rectangle {
        id: right_rect

        width: 260
        height: 34

        anchors.top: root.top
        anchors.topMargin: 3
        anchors.right: root.right
        anchors.rightMargin: 3

        color: !isFullChoosed? "#32394F": "transparent"
        radius: 37

        Label {
            id: right_label

            width: 260
            height: 15

            anchors.centerIn  : right_rect
            horizontalAlignment :  Text.AlignHCenter

            text: "Import watching-only wallet"

            color: !isFullChoosed? "#E2E7FF": "#7A88B0"

            font.pixelSize: 13
            font.family: "Roboto"
            font.weight: Font.Medium
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                isFullChoosed = false
                sig_full_changed(isFullChoosed)
            }
        }
    }

}
