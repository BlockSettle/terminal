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

// styled dialog popup
Dialog {
    x: (width > parent.width) ? 0 : (parent.width - width) / 2
    y: (height > parent.height) ? 0 : (parent.height - height) / 2

    focus: true
    modal: true
    closePolicy: Popup.NoAutoClose
    spacing: 0
    margins: 0

    background: Rectangle {
        color: BSStyle.backgroundColor
        border.color: BSStyle.dialogHeaderColor
        border.pixelAligned: true
    }
}
