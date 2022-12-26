/*

***********************************************************************************
* Copyright (C) 2018 - 2022, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.9
import QtQuick.Controls 2.3
import "../BsStyles"

Button {
    id: control
    property bool preferred: false

    font.pixelSize: 16
    font.family: "Roboto"
    font.weight: Font.Bold
    palette.buttonText: "#FFFFFF"

    icon.color: "transparent"
    icon.width: 24
    icon.height: 24

    background: Rectangle {
        implicitWidth: control.width
        implicitHeight: 50
        color: preferred ? (!control.enabled ? BSStyle.disabledColor :
                (control.down ? BSStyle.buttonsPreferredPressedColor :
                (control.hovered ? BSStyle.buttonsPreferredHoveredColor : BSStyle.buttonsPreferredColor))):
                (!control.enabled ? BSStyle.disabledColor :
                (control.down ? BSStyle.buttonsStandardPressedColor :
                (control.hovered ? BSStyle.buttonsStandardHoveredColor : BSStyle.buttonsStandardColor)))

        radius: 14
    }
}

