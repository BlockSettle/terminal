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
import "../../BsStyles"

PluginPopup {
   id: root
   property var controller: null

   background: Rectangle {
      width: root.width
      height: root.height
      color: "transparent"
   }

   contentItem: StackView {
      id: stackView
      initialItem: mainPage
      anchors.fill: parent
      
      SideShiftMainPage {
         id: mainPage
         onShift: {
            controller.inputCurrencySelected("USDT");   //TODO: should be a selected input currency from previous step
            if (mainPage.receive) {
               stackView.replace(buyPage)
            }
         }
         controller: root.controller
         visible: false
         anchors.centerIn: parent
      }

      SideShiftBuyPage {
         id: buyPage
         controller: root.controller
         visible: false

         inputCurrency: mainPage.inputCurrency
         outputCurrency: mainPage.outputCurrency
         receivingAddress: mainPage.receivingAddress
         anchors.centerIn: parent
      }
   }

   function reset()
   {
      mainPage.reset()
      stackView.replace(mainPage)
   }
}
