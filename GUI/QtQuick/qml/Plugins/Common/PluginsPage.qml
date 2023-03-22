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
import "../SideShift"
import "../../BsStyles"
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
               text: qsTr("Apps")
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
         cellHeight: 302
         model: pluginsListModel
         clip: true

         ScrollBar.vertical: ScrollBar { 
             id: verticalScrollBar
             policy: ScrollBar.AsNeeded
         }

         delegate: Rectangle {
            id: plugin_item
            color: "transparent"
            width: 237
            height: 302

            property var component
            property var plugin_popup

            Card {
               anchors.top: parent.top
               name: name_role
               description: description_role
               icon_source: icon_role
               onCardClicked: plugin_popup.open()
            }

            function finishCreation() {
               if (component.status == Component.Ready) {
                  plugin_popup = component.createObject(plugin_item)
                  plugin_popup.controller = pluginsListModel.getPlugin(index)
               }
               else if (component.status == Component.Error) {
                  console.log(component.errorString())
               }
            }

            Component.onCompleted: {
               component = Qt.createComponent(path_role);
               if (component.status == Component.Ready) {
                   finishCreation();
               }
               else if (component.status == Component.Error) {
                  console.log(component.errorString())
               }
               else {
                   component.statusChanged.connect(finishCreation);
               }
            }
         }
      }
   }
}
