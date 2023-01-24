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

    property var completer_vars: []

    radius: 14
    height: list.height + 30
    width: 170
    color : "#FFFFFF"

    ListView {
        id: list

        anchors.centerIn: parent
        spacing: 8

        keyNavigationEnabled: true

        model: completer_vars

        delegate: Label {
            id: _delegate

            width: 142
            text: completer_vars[index]

            color: "#020817"
            font.pixelSize: 16
            font.family: "Roboto"
            font.weight: Font.Normal
        }
    }
}
