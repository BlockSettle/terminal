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
    value: 0.5
    topPadding: 1
    bottomPadding: 1

    background: Rectangle {
        implicitWidth: 200
        implicitHeight: 6
        color: BSStyle.progressBarBgColor
        radius: 3
    }

    contentItem: Item {
        implicitWidth: 200
        implicitHeight: 4

        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: 2
            color: BSStyle.progressBarColor
        }
    }
}

