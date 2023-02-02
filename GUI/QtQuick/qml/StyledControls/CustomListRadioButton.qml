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

CustomListItem {
    id: root

    //properties
    property bool rad_but_down: rad_but.down

    signal sig_rad_but_clicked()

    icon_visible: false

    CustomRadioButton {
        id: rad_but

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.left: rect.left
        anchors.leftMargin: 21

        width: 15
        height: 15

        onClicked: root.sig_rad_but_clicked()
    }

}
