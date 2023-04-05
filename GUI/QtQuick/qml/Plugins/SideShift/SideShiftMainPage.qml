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
import "../../"


Item {
   id: root

   property var controller
   property bool receive: true
   property var receiveModel: controller.inputCurrenciesModel
   property var sendModel: controller.outputCurrenciesModel

   property alias inputCurrency: inputCombobox.currentText
   property alias outputCurrency: receivingCombobox.currentText
   property alias receivingAddress: addressCombobox.currentText

   signal shift()

   Column {
      anchors.centerIn: parent
      spacing: 20

      Text {
         text: controller.conversionRate
         color: "gray"
         font.pixelSize: 14
         font.family: "Roboto"
         anchors.horizontalCenter: parent.horizontalCenter
      }

      Row {
         spacing: 20
         anchors.horizontalCenter: parent.horizontalCenter

         SideShiftComboboxWithIcon {
            id: inputCombobox
            popupWidth: 400
            controlHint: qsTr("YOU SEND")
            model: root.receive ? root.receiveModel : root.sendModel
            enabled: root.receive

            onCurrentValueChanged: {
                root.controller.inputCurrencySelected(currentText)
                root.controller.inputNetwork = currentValue
            }
            onActivated: {
               root.controller.inputCurrencySelected(currentText)
               root.controller.inputNetwork = currentValue
            }

            onModelChanged: inputCombobox.currentIndex = 0
         }

         SideShiftIconButton {
            anchors.verticalCenter: parent.verticalCenter
            onButtonClicked: root.receive = !root.receive
         }

         SideShiftComboboxWithIcon {
            id: receivingCombobox
            popupWidth: 400
            controlHint: qsTr("YOU RECEIVE")
            model: root.receive ? root.sendModel : root.receiveModel 
            enabled: !root.receive
            onModelChanged: receivingCombobox.currentIndex = 0
         }
      }

      Item {
         width: 1
         height: 20
      }

      Text {
         text: qsTr("RECEIVING ADDRESS")
         color: "white"
         font.pixelSize: 20
         font.family: "Roboto"
         anchors.horizontalCenter: parent.horizontalCenter
      }

      SideShiftTextInput {
         id: addressInput
         visible: !root.receive
         textHint: qsTr("Your ") + receivingCombobox.currentText + qsTr(" address")
         anchors.horizontalCenter: parent.horizontalCenter
      }

      SideShiftCombobox {
         id: addressCombobox
         visible: root.receive
         model: addressListModel
         textRole: "address"
         valueRole: "address"
         anchors.horizontalCenter: parent.horizontalCenter
      }

      SideShiftButton {
         text: qsTr("SHIFT")
         enabled: root.receive ? addressCombobox.currentIndex >= 0 : addressInput.text !== ""
         anchors.horizontalCenter: parent.horizontalCenter
         onClicked: {
            if (root.controller.sendShift(addressCombobox.currentText)) {
                root.shift()
            }
         }
      }
   }

   function reset() {
      addressInput.text = ""
   }
}
