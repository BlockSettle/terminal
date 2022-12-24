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

Rectangle {
    id: rect

    property alias serial_num: serial_number.text
    property alias seed_text: seed.text

    width: 170
    height: 46

    color: "#020817"
    opacity: 1
    radius: 14

    Label {
        id: serial_number

        anchors.top: rect.top
        anchors.topMargin: 8
        anchors.left: rect.left
        anchors.leftMargin: 10

        font.pixelSize: 12
        font.family: "Roboto"
        font.weight: Font.Normal

        color: "#7A88B0"
    }

    Label {
        id: seed

        anchors.centerIn : rect

        font.pixelSize: 16
        font.family: "Roboto"
        font.weight: Font.Normal

        color: "#E2E7FF"
    }
}
