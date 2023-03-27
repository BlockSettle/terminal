/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.15
import QtQuick.Controls 2.3

import "../Styles"

Button {
    id: control

    width: 150
    height: 50
    focusPolicy: Qt.NoFocus

    font.pixelSize: 16
    font.family: "Roboto"
    font.weight: Font.Bold
    font.letterSpacing: 0.5

    hoverEnabled: true
    activeFocusOnTab: true

    background: Rectangle {
        id: backgroundItem
        color: "transparent"
        radius: 8
        border.width: 1
        border.color: SideSwapStyles.buttonBackground
    }

    contentItem: Text {
        text: control.text
        font: control.font
        anchors.fill: parent
        color: SideSwapStyles.primaryTextColor
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
    }
}
