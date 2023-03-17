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

TextArea {
    id: root
    horizontalAlignment: Text.AlignHLeft
    font.pixelSize: 11
    font.family: "Roboto"
    color: "white"
    wrapMode: TextEdit.WordWrap

    background: Rectangle {
        implicitWidth: 200
        implicitHeight: 50
        color:"transparent"
        border.color: BSStyle.inputsBorderColor
    }

    CustomContextMenu {
        anchors.fill: parent
    }
}
