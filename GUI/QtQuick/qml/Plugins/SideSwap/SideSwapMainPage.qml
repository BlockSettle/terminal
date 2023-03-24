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

   property bool peg_in: true

   Rectangle{
      anchors.fill: parent
      color: "transparent"


      Rectangle {
         id: topPanel
         width: parent.width
         height: 350
         anchors.top: parent.top
         color: SideSwapStyles.darkBlueBackground
      }

      Rectangle {
         width: parent.width
         height: parent.height - topPanel.height
         anchors.bottom: parent.bottom
         color: SideSwapStyles.skyBlueBackground
      }

      Column {
         spacing: 20
         anchors.fill: parent
         anchors.topMargin: 40
         anchors.rightMargin: 20
         anchors.leftMargin: 20

         CustomSwitch {
            width: parent.width
            height: 30
            anchors.horizontalCenter: parent.horizontalCenter
            checked: root.peg_in
            
            onCheckedChanged: root.peg_in = checked
         }

         CurrencyLabel {
            header_text: qsTr("Deliver")
            currency: root.peg_in ? qsTr("BTC") : qsTr("L-BTC")
            currency_icon: root.peg_in 
               ? "qrc:/images/sideswap/btc_icon.svg"
               : "qrc:/images/sideswap/lbtc_icon.svg"
            comment: qsTr("Min: 0.001 ") + currency
         }

         Rectangle {
            height: 2
            width: parent.width
            color: SideSwapStyles.spacerColor
         }

         Text {
            text: "SideSwap will generate a Peg-In address for you to deliver BTC into. Each peg-in/out URL is unique and can be re-entered to view your progress. A peg-in/out address may be re-used."
            width: parent.width - 20
            color: SideSwapStyles.parapraphTextColor
            clip: true
            wrapMode: Text.Wrap
         }

         Row {
            spacing: 20
            width: parent.width
            height: 50

            IconButton {
               width: 50
               height: 50
               radius: 25

               onButtonClicked: root.peg_in = !root.peg_in
            }

            Rectangle {
               width: 200
               height: 30
               radius: 15
               color: "lightblue"
               anchors.verticalCenter: parent.verticalCenter

               Rectangle {
                  width: 15
                  height: 15
                  anchors.left: parent.left
                  anchors.top: parent.top
                  color: "lightblue"
               }

               Text {
                  text: qsTr("Covertion rate 99.9%")
                  color: "black"
                  anchors.centerIn: parent
               }
            }
         }

         Row {
            spacing: 20
            height: 60
            width: parent.width
            
            CurrencyLabel {
               id: receiveLabel
               header_text: qsTr("Receive")
               currency: root.peg_in ? qsTr("L-BTC") : qsTr("BTC")
               currency_icon: root.peg_in 
                  ? "qrc:/images/sideswap/lbtc_icon.svg"
                  : "qrc:/images/sideswap/btc_icon.svg"
               comment: ""
               width: 100
            }

            CustomCombobox {
               model: ['A', 'B', 'C']
               height: 60
               width: parent.width - receiveLabel.width - 20
               anchors.verticalCenter: parent.verticalCenter
               visible: !root.peg_in
               comboboxHint: qsTr("Fee suggestion")
            }
         }

         Rectangle {
            height: 2
            width: parent.width
            color: SideSwapStyles.spacerColor
         }

         Column {
            width: parent.width
            spacing: 5

            CustomCombobox {
               height: 60
               width: parent.width
               model: walletBalances
               textRole: "name"
               valueRole: "name"
               comboboxHint: qsTr("Wallet")
            }

            CustomTextEdit {
               width: parent.width
               height: 60
               textHint: qsTr("Amount")
               visible: root.peg_in
               inputHints: Text.ImhDigitsOnly
            }

            CustomTextEdit {
               width: parent.width
               height: 60
               textHint: qsTr("Your Liquid Address")
               visible: root.peg_in
            }
         }

         CustomButton {
            text: qsTr("CONTINUE")
            width: parent.width
            anchors.horizontalCenter: parent.horizontalCenter
         }
      }
   }

   function reset() {
   }
}
