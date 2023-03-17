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
import "../../BsStyles"

Rectangle {
   id: control
   width: 217
   height: 292
   color: "transparent"
   radius: 14
   border.width: 1
   border.color: BSStyle.defaultGreyColor

   property alias name: title_item.text 
   property alias description: description_item.text
   property alias icon_source: card_icon.source

   signal cardClicked()

   Column {
      spacing: 8
      anchors.fill: parent
      anchors.margins: 12

      Image {
         id: card_icon
         width: 193
         height: 122
      }

      Text {
         id: title_item
         topPadding: 10
         font.family: "Roboto"
         font.pixelSize: 16
         font.weight: Font.DemiBold
         font.letterSpacing: 0.3
         color: BSStyle.titanWhiteColor
         width: parent.width
      }

      Text {
         id: description_item
         font.family: "Roboto"
         font.pixelSize: 14
         font.letterSpacing: 0.3
         color: BSStyle.titleTextColor
         width: parent.width
         wrapMode: Text.WrapAnywhere
      }
   }

   MouseArea {
      id: mouse_area
      anchors.fill: parent
      hoverEnabled: true

      onClicked: control.cardClicked()
   }
}

