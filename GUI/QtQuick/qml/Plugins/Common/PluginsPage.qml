/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.9
import QtQuick.Controls 2.3

import "."
import "../../StyledControls"

Rectangle {
   id: root
   width: 1200
   height: 768
   color: "transparent"

   Column {
      anchors.fill: parent
      anchors.leftMargin: 14

      Rectangle {
         id: header
         width: parent.width
         height: 76
         color: "transparent"

         Row {
            spacing: 6
            anchors.fill: parent

            Text {
               height: parent.height
               width: 110
               text: qsTr("Plugins")
               color: BSStyle.textColor
               font.family: "Roboto"
               font.pixelSize: 20
               font.weight: Font.Bold
               font.letterSpacing: 0.35
               horizontalAlignment: Text.AlignHCenter
               verticalAlignment: Text.AlignVCenter
            }
         }
      }

      GridView {
         width: parent.width
         height: parent.height - header.height
         cellWidth: 237
         cellHeight: 312
         model: pluginFilterModel
         clip: true

         ScrollBar.vertical: ScrollBar { 
             id: verticalScrollBar
             policy: ScrollBar.AsNeeded
         }

         delegate: Rectangle {
            color: "transparent"
            width: 237
            height: 312

            Card {
               anchors.centerIn: parent
               name: name
               description: description
               icon_source: icon
            }
         }
      }
   }
}