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
import "."

CustomComboBox {
    id: control

    font.family: "Roboto"
    font.letterSpacing: 0.3
    fontSize: BSSizes.applyScale(12)
    fontColor: BSStyle.titleTextColor

    leftPadding: BSSizes.applyScale(10)
    rightPadding: 0
    topPadding: 0
    bottomPadding: BSSizes.applyScale(2)

    indicator: Image {
        width: BSSizes.applyScale(6)
        height: BSSizes.applyScale(3)
        anchors.verticalCenter: parent.verticalCenter
        source: "qrc:/images/combobox_open_button.svg"

        anchors.right: parent.right
        anchors.rightMargin: BSSizes.applyScale(14)
    }
}

