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


RadioButton {
    id: control

    checked: true
    activeFocusOnTab: false

    indicator: Rectangle {
        implicitWidth: BSSizes.applyScale(16)
        implicitHeight: BSSizes.applyScale(16)
        x: control.leftPadding
        y: parent.height / 2 - height / 2
        radius: BSSizes.applyScale(8)
        border.color: "#45A6FF"
        color: "transparent"

        Rectangle {
            width: BSSizes.applyScale(8)
            height: BSSizes.applyScale(8)
            x: BSSizes.applyScale(4)
            y: BSSizes.applyScale(4)
            radius: BSSizes.applyScale(4)
            color: "#45A6FF"
            visible: control.checked
        }
    }

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.checked ? "#E2E7FF" : "#7A88B0"
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + control.spacing
    }
}
