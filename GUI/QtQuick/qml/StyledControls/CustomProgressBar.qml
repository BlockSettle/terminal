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

ProgressBar {
    id: control
    padding: 0

    background: Rectangle {
        implicitWidth: BSSizes.applyScale(532)
        implicitHeight: BSSizes.applyScale(8)
        color: "transparent"
        radius: BSSizes.applyScale(32)

        border.width: BSSizes.applyScale(1)
        border.color: "#3C435A"
    }

    contentItem: Item {
        implicitWidth: BSSizes.applyScale(532)
        implicitHeight: BSSizes.applyScale(8)

        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: BSSizes.applyScale(32)
            color: "#45A6FF"
        }
    }
}

