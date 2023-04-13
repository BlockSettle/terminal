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
import "../../../BsStyles"

Button {
    id: control

    width: BSSizes.applyScale(150)
    height: BSSizes.applyScale(50)

    focusPolicy: Qt.NoFocus
    property bool active: true

    font.pixelSize: BSSizes.applyScale(16)
    font.family: "Roboto"
    font.letterSpacing: 0.5

    hoverEnabled: true
    activeFocusOnTab: true

    background: Rectangle {
        id: backgroundItem
        color: control.active ? SideSwapStyles.buttonBackground : SideSwapStyles.buttonSecondaryBackground
        radius: BSSizes.applyScale(8)
    }

    contentItem: Text {
        text: control.text
        font: control.font
        anchors.fill: parent
        color: control.active ? SideSwapStyles.primaryTextColor : SideSwapStyles.paragraphTextColor
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
    }
}
