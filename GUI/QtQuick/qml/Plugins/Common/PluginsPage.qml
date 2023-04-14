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
   color: BSStyle.backgroundColor

   Column {
      anchors.fill: parent
      anchors.leftMargin: BSSizes.applyScale(14)

      Rectangle {
         id: header
         width: parent.width
         height: BSSizes.applyScale(76)
         color: "transparent"

         Row {
            spacing: BSSizes.applyScale(6)
            anchors.fill: parent

            Text {
               height: parent.height
               width: BSSizes.applyScale(110)
               text: qsTr("Apps")
               color: BSStyle.textColor
               font.family: "Roboto"
               font.pixelSize: BSSizes.applyScale(20)
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
         cellWidth: BSSizes.applyScale(237)
         cellHeight: BSSizes.applyScale(302)
         model: pluginsListModel
         clip: true

         ScrollBar.vertical: ScrollBar { 
             id: verticalScrollBar
             policy: ScrollBar.AsNeeded
         }

         delegate: Rectangle {
            id: plugin_item
            color: "transparent"
            width: BSSizes.applyScale(237)
            height: BSSizes.applyScale(302)

            property var component
            property var plugin_popup

            Card {
               anchors.top: parent.top
               name: name_role
               description: description_role
               icon_source: icon_role
               onCardClicked: {
                   plugin_popup.reset()
                   plugin_popup.controller = pluginsListModel.getPlugin(index)
                   plugin_popup.controller.init()
                   plugin_popup.open()
               }
            }

            function finishCreation() {
                plugin_popup = component.createObject(plugin_item)
            }

            Component.onCompleted: {
               component = Qt.createComponent(path_role);
               if (component !== null) {
                  if (component.status === Component.Ready) {
                      finishCreation();
                  }
                  else if (component.status === Component.Error) {
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
}
