/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.15
import QtQuick.Window 2.2
import QtQuick.Controls 2.12

import "../../StyledControls"

Popup {
    id: plugin_popup
    width: 580
    height: 720
    anchors.centerIn: Overlay.overlay

    property var controller: null

    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    CloseIconButton {
        anchors.right: parent.right
        anchors.top: parent.top
        onClose: plugin_popup.close()
    }
}
