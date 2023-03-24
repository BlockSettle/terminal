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

import "."
import "../Common"
import "../../"

PluginPopup {
   id: root

   background: Rectangle {
      anchors.fill: parent
      color: "black"
      radius: 14
   }

   contentItem: StackView {
      id: stackView
      initialItem: mainPage
      anchors.fill: parent

      SideSwapMainPage {
         id: mainPage
      }
   }

   function reset()
   {
   }
}
