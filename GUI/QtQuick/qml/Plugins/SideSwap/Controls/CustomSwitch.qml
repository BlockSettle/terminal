/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.15
import QtQuick.Controls 2.3

import "../Styles"

Rectangle {
   id: root
   color: "transparent"

   property bool checked: true

   Row {
      anchors.fill: parent

      CustomButton {
         text: qsTr("PEG-IN")
         width: parent.width / 2
         height: parent.height
         active: root.checked
         onClicked: root.checked = true
      }
      CustomButton {
         text: qsTr("PEG-OUT")
         width: parent.width / 2
         height: parent.height
         active: !root.checked
         onClicked: root.checked = false
      }
   }
}
