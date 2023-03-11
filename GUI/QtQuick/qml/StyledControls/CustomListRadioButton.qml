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
    property alias radio_checked: rad_but.checked
    property var radio_group

    signal sig_radio_clicked()

    icon_visible: false

    CustomRadioButton {
        id: rad_but

        ButtonGroup.group: radio_group

        anchors.verticalCenter: root.verticalCenter
        anchors.left: root.left
        anchors.leftMargin: 21

        leftPadding: 0
        spacing: 0

        width: 15
        height: 15

        onClicked : root.sig_radio_clicked()
    }

    onClicked : {
        rad_but.checked = true
        root.sig_radio_clicked()
    }
}
