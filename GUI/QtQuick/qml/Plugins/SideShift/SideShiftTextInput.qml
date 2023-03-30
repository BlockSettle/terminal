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

Rectangle {
   id: control
   width: 400
   height: 50
   radius: 4
   color: "#181414"
   border.width: 1
   border.color: (mouseArea.containsMouse || control.activeFocus) ? "white" : "gray"
   activeFocusOnTab: true

   property string textHint
   property string fontFamily
   property alias text: textEdit.text

   TextInput {
      id: textEdit
      color: "white"
      verticalAlignment: Text.AlignVCenter
      horizontalAlignment: Text.AlignHCenter
      anchors.fill: parent
      font.family: control.fontFamily
      width: parent.width
      clip: true

      Text {
         text: control.textHint
         color: "white"
         anchors.centerIn: parent
         font.family: control.fontFamily
         visible: textEdit.text == "" && !textEdit.activeFocus
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