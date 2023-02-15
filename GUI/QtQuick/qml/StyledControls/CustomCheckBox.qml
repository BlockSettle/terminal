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


CheckBox {
    id: control

    checked: true
    spacing: 0

    indicator: Rectangle {
        implicitWidth: 18
        implicitHeight: 18
        x: control.leftPadding
        y: parent.height / 2 - height / 2
        radius: 6
        border.color: "#416485"
        color: "transparent"

        Image {
            id: check_icon

            anchors.centerIn: parent

            width: 10
            height: 7
            sourceSize.width: 10
            sourceSize.height: 7

            visible: control.checked
            source: "qrc:/images/check.svg"
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
