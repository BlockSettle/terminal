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

   property string minTransferAmount: qsTr("0.001")
   property string toAddress: "bc1qvrl85pygns90xut25qu0tpmawm9h03j3d9w94a"
   property string fromAddress: "bc1qvrl85pygns90xut25qu0tpmawm9h03j3d9w94a"

   signal back()

   Rectangle {
      anchors.fill: parent
      color: SideSwapStyles.darkBlueBackground
   }

   Column {
      spacing: 30
      anchors.fill: parent
      anchors.margins: 20

      Text {
         text: qsTr("Send L-BTC to the following address:")
         color: SideSwapStyles.primaryTextColor
         font.pixelSize: 24
         anchors.horizontalCenter: parent.horizontalCenter
         topPadding: 20
      }

      Text {
         text: qsTr("Min amount: ") + minTransferAmount
         color: SideSwapStyles.primaryTextColor
         font.pixelSize: 14
         anchors.horizontalCenter: parent.horizontalCenter
      }

      Rectangle {
         height: 1
         width: parent.width
         color: SideSwapStyles.spacerColor
      }

      Row {
         spacing: 20
         width: parent.width
         height: 300

         Item {
            width: parent.width / 2 -  parent.spacing
            height: parent.height

            Rectangle {
               width: 240
               height: 240
               radius: 20
               color: SideSwapStyles.buttonBackground
               anchors.centerIn: parent

               Rectangle {
                  width: 226
                  height: 226
                  radius: 16
                  anchors.centerIn: parent

                  Image {
                      source: "image://QR/" + root.toAddress
                      sourceSize.width: 220
                      sourceSize.height: 220
                      width: 220
                      height: 220
                      anchors.centerIn: parent
                  }
               }
            }
         }  

         Item {
            width: parent.width / 2 -  parent.spacing
            height: parent.height

            Column {
               spacing: 20
               anchors.fill: parent
               anchors.margins: 20

               Text {
                  text: root.toAddress
                  color: SideSwapStyles.primaryTextColor
                  font.pixelSize: 18
                  width: parent.width
                  clip: true
                  wrapMode: Text.Wrap
               }

               CustomButton {
                  text: qsTr("Copy Address")
                  width: parent.width
                  height: 50
                  anchors.horizontalCenter: parent.horizontalCenter
                  onClicked: bsApp.copyAddressToClipboard(root.toAddress)
               }
            }
         }
      }

      Rectangle {
         width: parent.width
         height: 1
         color: SideSwapStyles.spacerColor
      }

      Column {
         Text {
            text: qsTr("BTC payment address")
            font.pixelSize: 14
            color: SideSwapStyles.paragraphTextColor
         }

         Text {
            text: root.fromAddress
            font.pixelSize: 14
            color: SideSwapStyles.paragraphTextColor
         }
      }

      CustomBorderedButton {
         text: qsTr("BACK")
         width: parent.width
         height: 60
         anchors.horizontalCenter: parent.horizontalCenter
         onClicked: root.back()
      }
   }
}
