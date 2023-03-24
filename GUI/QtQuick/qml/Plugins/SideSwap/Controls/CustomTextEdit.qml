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

Rectangle {
   id: control
   width: 400
   height: 50
   radius: 12
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
      leftPadding: 10
      topPadding: 32
      rightPadding: 10
      bottomPadding: 10
      clip: true
      anchors.fill: parent
      font.family: control.fontFamily

      Text {
         text: control.textHint
         color: SideSwapStyles.secondaryTextColor
         font.family: control.fontFamily
         leftPadding: 10
         topPadding: 10
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