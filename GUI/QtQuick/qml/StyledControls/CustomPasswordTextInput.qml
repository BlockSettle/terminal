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


TextField {
    property bool allowShowPass: true

    horizontalAlignment: Text.AlignHLeft
    font.pixelSize: BSSizes.applyScale(11)
    color: BSStyle.inputsFontColor
    padding: 0
    echoMode: button.pressed ? TextInput.Normal : TextInput.Password
    selectByMouse: false

    background: Rectangle {
        implicitWidth: BSSizes.applyScale(200)
        implicitHeight: BSSizes.applyScale(25)
        color:"transparent"
        border.color: BSStyle.inputsBorderColor

        Button {
            id: button
            visible: allowShowPass
            contentItem: Rectangle {
                color: "transparent"
                Image {
                    fillMode: Image.PreserveAspectFit
                    anchors.fill: parent
                    source: "qrc:/resources/eye.png"
                }
            }
            padding: (button.pressed ? 1 : 0) - BSSizes.applyScale(2)
            background: Rectangle {color: "transparent"}
            anchors.right: parent.right
            width: BSSizes.applyScale(23)
            height: BSSizes.applyScale(23)
        }
    }
}
