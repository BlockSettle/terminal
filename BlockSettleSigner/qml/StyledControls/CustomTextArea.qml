/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.9
import QtQuick.Controls 2.3
import "../BsStyles"

TextArea {
    horizontalAlignment: Text.AlignHLeft
    font.pixelSize: 11
    color: "white"
    wrapMode: TextEdit.WordWrap

    background: Rectangle {
        implicitWidth: 200
        implicitHeight: 50
        color:"transparent"
        border.color: BSStyle.inputsBorderColor
    }
}
