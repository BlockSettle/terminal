/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.15
import QtQuick.Window 2.2
import QtQuick.Controls 2.12

import "../Styles"
import "../../../BsStyles"

Rectangle {
   id: control
   width: BSSizes.applyScale(400)
   height: BSSizes.applyScale(50)
   radius: BSSizes.applyScale(12)
   color: SideSwapStyles.darkBlueBackground
   border.width: 1
   border.color: textEdit.activeFocus ? SideSwapStyles.buttonBackground : SideSwapStyles.spacerColor
   activeFocusOnTab: true

   property string textHint
   property string fontFamily
   property alias text: textEdit.text
   property alias inputHints: textEdit.inputMethodHints

   TextInput {
      id: textEdit
      color: "white"
      leftPadding: BSSizes.applyScale(10)
      topPadding: BSSizes.applyScale(32)
      rightPadding: BSSizes.applyScale(10)
      bottomPadding: BSSizes.applyScale(10)
      clip: true
      anchors.fill: parent
      font.pixelSize: BSSizes.applyScale(14)
      font.family: "Roboto"

      Text {
         text: control.textHint
         color: SideSwapStyles.secondaryTextColor
         font.family: "Roboto"
         font.weight: Font.Bold
         font.pixelSize: BSSizes.applyScale(12)
         leftPadding: BSSizes.applyScale(10)
         topPadding: BSSizes.applyScale(10)
      }
   }

   MouseArea {
      id: mouseArea
      anchors.fill: parent
      hoverEnabled: true
      onClicked: {
          textEdit.forceActiveFocus()
          mouse.accepted = false
      }
   }
}
