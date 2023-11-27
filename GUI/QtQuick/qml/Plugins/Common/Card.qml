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
   width: BSSizes.applyScale(217)
   height: BSSizes.applyScale(292)
   color: "transparent"
   radius: BSSizes.applyScale(14)
   border.width: 1
   border.color: BSStyle.defaultGreyColor

   property alias name: title_item.text 
   property alias description: description_item.text
   property alias icon_source: card_icon.source

   signal cardClicked()

   Column {
      spacing: BSSizes.applyScale(8)
      anchors.fill: parent
      anchors.margins: BSSizes.applyScale(12)

      Image {
         id: card_icon
         width: BSSizes.applyScale(193)
         height: BSSizes.applyScale(122)
      }

      Text {
         id: title_item
         topPadding: BSSizes.applyScale(10)
         font.family: "Roboto"
         font.pixelSize: BSSizes.applyScale(16)
         font.weight: Font.DemiBold
         font.letterSpacing: 0.3
         color: BSStyle.titanWhiteColor
         width: parent.width
      }

      Text {
         id: description_item
         font.family: "Roboto"
         font.pixelSize: BSSizes.applyScale(14)
         font.letterSpacing: 0.3
         color: BSStyle.titleTextColor
         width: parent.width
         wrapMode: Text.Wrap
      }
   }

   MouseArea {
      id: mouse_area
      anchors.fill: parent
      hoverEnabled: true

      onClicked: control.cardClicked()
   }
}

