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
import QtQuick.Layouts 1.3
import "../BsStyles"


ToolButton {
    id: control

    property bool _isSelected: false

    Layout.fillHeight: true
    implicitWidth: BSSizes.applyScale(110)

    hoverEnabled: true

    font.pixelSize: BSSizes.applyScale(12)
    font.family: "Roboto"
    font.weight: Font.Normal
    palette.buttonText: !enabled ? BSStyle.buttonsDisabledTextColor :
                        (control.down ?  BSStyle.textPressedColor :
                        (_isSelected ? BSStyle.selectedColor : BSStyle.titleTextColor))

    icon.color: palette.buttonText
    icon.width: BSSizes.applyScale(16)
    icon.height: BSSizes.applyScale(16)

    background: Rectangle {
        anchors.fill: parent
        id: btn_background
        clip: true
        color: !control.enabled ? BSStyle.buttonsDisabledColor :
                (control.down ? BSStyle.buttonsPressedColor :
                (control.hovered ? BSStyle.buttonsHoveredColor : BSStyle.buttonsMainColor))
        Rectangle {
            color: 'transparent'
            anchors.fill: parent
            anchors.rightMargin: -border.width
            anchors.topMargin:  -border.width
            anchors.bottomMargin:  -border.width
            border.width: BSSizes.applyScale(1)
            border.color: BSStyle.defaultBorderColor
        }
    }

    function select(isSelected: bool)
    {
        _isSelected = isSelected;
    }
}
