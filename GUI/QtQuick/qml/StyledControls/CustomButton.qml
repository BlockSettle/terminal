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

    height: 50

    property bool preferred: false

    //aliases
    property alias back_radius: back.radius

    activeFocusOnTab: true

    font.pixelSize: 16
    font.family: "Roboto"
    font.weight: Font.Bold
    palette.buttonText: enabled ? BSStyle.buttonsTextColor : BSStyle.buttonsDisabledTextColor


    icon.color: "transparent"
    icon.width: 24
    icon.height: 24

    background: Rectangle {

        id: back

        implicitWidth: control.width
        implicitHeight: control.height

        color: preferred ? (!control.enabled ? BSStyle.buttonsDisabledColor :
                (control.down ? BSStyle.buttonsPreferredPressedColor :
                (control.hovered ? BSStyle.buttonsPreferredHoveredColor : BSStyle.buttonsPreferredColor))):
                (!control.enabled ? BSStyle.buttonsDisabledColor :
                (control.down ? BSStyle.buttonsStandardPressedColor :
                (control.hovered ? BSStyle.buttonsStandardHoveredColor : BSStyle.buttonsStandardColor)))

        radius: 14

        border.color: preferred ? BSStyle.buttonsPreferredBorderColor : BSStyle.buttonsStandardBorderColor
        border.width: control.activeFocus? 1 : 0

    }

    Keys.onEnterPressed: {
        click_enter()
    }

    Keys.onReturnPressed: {
        click_enter()
    }

    onClicked: {
        click_enter()
    }
}

