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
   property var receiveModel: [
        { 
            currency: "BTC",
            icon: "qrc:/images/sideshift_btc.png"
        },
        { 
            currency: "ETC",
            icon: "qrc:/images/sideshift_btc.png"
        }
    ]
   property var sendModel: [
        { 
            currency: "BTC",
            icon: "qrc:/images/sideshift_btc.png"
        }
    ]

   signal shift()

   Column {
      anchors.centerIn: parent
      spacing: 20

      Text {
         text: root.controller !== null ? root.controller.conversionRate : ""
         color: "gray"
         font.pixelSize: 14
         font.family: "Roboto"
         anchors.horizontalCenter: parent.horizontalCenter
      }

      Row {
         spacing: 20
         anchors.horizontalCenter: parent.horizontalCenter

         SideShiftComboboxWithIcon {
            popupWidth: 200
            textRole: "currency"
            controlHint: qsTr("YOU SEND")
            model: root.receive ? root.receiveModel : root.sendModel
         }

         SideShiftIconButton {
            anchors.verticalCenter: parent.verticalCenter
            onButtonClicked: root.receive = !root.receive
         }

         SideShiftComboboxWithIcon {
            id: receivingCombobox
            popupWidth: 200
            textRole: "currency"
            controlHint: qsTr("YOU RECEIVE")
            model: root.receive ? root.sendModel : root.receiveModel 
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
         id: walletCombobox
         visible: root.receive
         model: walletBalances
         textRole: "name"
         valueRole: "name"
         anchors.horizontalCenter: parent.horizontalCenter
      }

      SideShiftButton {
         text: qsTr("SHIFT")
         enabled: root.receive ? walletCombobox.currentIndex >= 0 : addressInput.text !== ""
         anchors.horizontalCenter: parent.horizontalCenter
         onClicked: root.shift()
      }
   }
}
