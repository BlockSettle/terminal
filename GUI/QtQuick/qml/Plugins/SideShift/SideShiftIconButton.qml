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
   width: 40
   height: 40
   color: "transparent"
   activeFocusOnTab: true

   signal buttonClicked()

   Image {
      anchors.fill: parent
      source: "qrc:/images/transfer_icon.png"
   }

   MouseArea {
      id: mouseArea
      anchors.fill: parent
      hoverEnabled: true
      onClicked: {
         control.buttonClicked()
      }
   }
}