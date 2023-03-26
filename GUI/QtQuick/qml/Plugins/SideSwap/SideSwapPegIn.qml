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
import "Styles"
import "Controls"
import "../../"


Item {
   id: root
   width: 620
   height: 720

   signal back()

   Rectangle {
      anchors.fill: parent
      color: SideSwapStyles.darkBlueBackground
   }

   Column {
      anchors.fill: parent
      anchors.margins: 20

      CustomBorderedButton {
         width: parent.width
         text: qsTr("BACK")
         onClicked: root.back()
      }
   }
}
