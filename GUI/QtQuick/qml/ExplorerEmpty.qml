/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQml.Models 2

import "StyledControls"
import "BsStyles"
//import "BsControls"
//import "BsDialogs"
//import "js/helper.js" as JsHelper

Item {
    Column {
        spacing: 32
        anchors.fill: parent

        Label {
            text: " "
            font.pointSize: 50
            height: 50
        }
        Image {
            width: 150
            height: 150
            source: "qrc:/images/logo.png"
            Layout.topMargin: 150
            //Layout.horizontalCenter: parent
        }
        Label {
            text: qsTr("<font color=\"darkgrey\">Provides you with a convenient, powerful, yet simple tool to read<BR> transaction and address data from the bitcoin network</font>")
            font.pointSize: 12
        }

    }
}
