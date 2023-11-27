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

    width: BSSizes.applyScale(530)
    height: BSSizes.applyScale(40)

    color: "transparent"
    radius: BSSizes.applyScale(37)
    border.color : BSStyle.defaultBorderColor
    border.width : BSSizes.applyScale(1)

    Rectangle {
        id: left_rect

        width: BSSizes.applyScale(260)
        height: BSSizes.applyScale(34)

        anchors.top: root.top
        anchors.topMargin: BSSizes.applyScale(3)
        anchors.left: root.left
        anchors.leftMargin: BSSizes.applyScale(3)

        color: isFullChoosed? "#32394F": "transparent"
        radius: BSSizes.applyScale(37)

        Label {
            id: left_label

            width: BSSizes.applyScale(260)
            height: BSSizes.applyScale(15)

            anchors.centerIn  : left_rect
            horizontalAlignment :  Text.AlignHCenter

            text: "Full"

            color: isFullChoosed? "#E2E7FF": "#7A88B0"

            font.pixelSize: BSSizes.applyScale(13)
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

        width: BSSizes.applyScale(260)
        height: BSSizes.applyScale(34)

        anchors.top: root.top
        anchors.topMargin: BSSizes.applyScale(3)
        anchors.right: root.right
        anchors.rightMargin: BSSizes.applyScale(3)

        color: !isFullChoosed? "#32394F": "transparent"
        radius: BSSizes.applyScale(37)

        Label {
            id: right_label

            width: BSSizes.applyScale(260)
            height: BSSizes.applyScale(15)

            anchors.centerIn  : right_rect
            horizontalAlignment :  Text.AlignHCenter

            text: "Import watching-only wallet"

            color: !isFullChoosed? "#E2E7FF": "#7A88B0"

            font.pixelSize: BSSizes.applyScale(13)
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
