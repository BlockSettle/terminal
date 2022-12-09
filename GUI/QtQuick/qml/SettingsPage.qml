/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2
import QtQuick.Controls 2.9
import QtQuick.Layouts 1.3
import QtQml.Models 2

import "StyledControls"
import "BsStyles"
//import "BsControls"
//import "BsDialogs"
//import "js/helper.js" as JsHelper

Item {
    id: settings

    Column {
        spacing: 50
        anchors.fill: parent

        Button {
            icon.source: "qrc:/images/send_icon.png"
            onClicked: {
                stack.pop()
            }
        }
        Text {
            text: qsTr("<font color=\"cyan\">Settings page</font>")
            font.pointSize: 23
        }
        Image {
            source: "qrc:/images/bs_logo.png"
        }
    }
}
