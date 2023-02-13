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
//    property bool qmlTitleVisible: true     //!mainWindow.isLiteMode
    property string text
    clip: true
    color: "transparent"
    height: 40

    Rectangle {
//        visible: qmlTitleVisible
//        height: qmlTitleVisible ? 40 : 0
        anchors.fill: parent
        color: BSStyle.dialogHeaderColor
    }

    Text {
//        visible: qmlTitleVisible
//        height: qmlTitleVisible ? 40 : 0
        anchors.fill: parent
        leftPadding: 10
        rightPadding: 10

        text: parent.text
        font.capitalization: Font.AllUppercase
        color: BSStyle.textColor
        font.pixelSize: 11
        verticalAlignment: Text.AlignVCenter
    }
}
