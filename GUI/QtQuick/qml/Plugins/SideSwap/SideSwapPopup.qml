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

   background: Rectangle {
      anchors.fill: parent
      color: "black"
      radius: BSSizes.applyScale(14)
   }

   contentItem: StackView {
      id: stackView
      initialItem: mainPage
      anchors.fill: parent

      SideSwapMainPage {
         id: mainPage
         visible: false
         anchors.centerIn: parent

         onContinueClicked: {
            if (mainPage.peg_in) {
               stackView.replace(pegInPage)
            }
            else {
               stackView.replace(pegOutPage)
            }
         }
      }

      SideSwapPegOut {
         id: pegOutPage
         visible: false
         onBack: root.reset()
         anchors.centerIn: parent
      }

      SideSwapPegIn {
         id: pegInPage
         visible: false
         onBack: root.reset()
         anchors.centerIn: parent
      }
   }

   function reset()
   {
      stackView.replace(mainPage)
      mainPage.reset()
   }
}
