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
import "../../BsStyles"


Item {
   id: root
   width: BSSizes.applyWindowWidthScale(620)
   height: BSSizes.applyWindowHeightScale(720)

   property string minTransferAmount: qsTr("0.001")
   property string toAddress: "bc1qvrl85pygns90xut25qu0tpmawm9h03j3d9w94a"
   property string fromAddress: "bc1qvrl85pygns90xut25qu0tpmawm9h03j3d9w94a"

   signal back()

   Rectangle {
      anchors.fill: parent
      color: SideSwapStyles.darkBlueBackground
   }

   Column {
      spacing: BSSizes.applyScale(30)
      anchors.fill: parent
      anchors.margins: BSSizes.applyScale(20)

      Text {
         text: qsTr("Send L-BTC to the following address:")
         color: SideSwapStyles.primaryTextColor
         font.pixelSize: BSSizes.applyScale(24)
         anchors.horizontalCenter: parent.horizontalCenter
         topPadding: BSSizes.applyScale(20)
      }

      Text {
         text: qsTr("Min amount: ") + minTransferAmount
         color: SideSwapStyles.primaryTextColor
         font.pixelSize: BSSizes.applyScale(14)
         anchors.horizontalCenter: parent.horizontalCenter
      }

      Rectangle {
         height: 1
         width: parent.width
         color: SideSwapStyles.spacerColor
      }

      Row {
         spacing: BSSizes.applyScale(20)
         width: parent.width
         height: BSSizes.applyScale(300)

         Item {
            width: parent.width / 2 -  parent.spacing
            height: parent.height

            Rectangle {
               width: BSSizes.applyScale(240)
               height: BSSizes.applyScale(240)
               radius: BSSizes.applyScale(20)
               color: SideSwapStyles.buttonBackground
               anchors.centerIn: parent

               Rectangle {
                  width: BSSizes.applyScale(226)
                  height: BSSizes.applyScale(226)
                  radius: BSSizes.applyScale(16)
                  anchors.centerIn: parent

                  Image {
                      source: "image://QR/" + root.toAddress
                      sourceSize.width: BSSizes.applyScale(220)
                      sourceSize.height: BSSizes.applyScale(220)
                      width: BSSizes.applyScale(220)
                      height: BSSizes.applyScale(220)
                      anchors.centerIn: parent
                  }
               }
            }
         }  

         Item {
            width: parent.width / 2 -  parent.spacing
            height: parent.height

            Column {
               spacing: BSSizes.applyScale(20)
               anchors.fill: parent
               anchors.margins: BSSizes.applyScale(20)

               Text {
                  text: root.toAddress
                  color: SideSwapStyles.primaryTextColor
                  font.pixelSize: BSSizes.applyScale(18)
                  width: parent.width
                  clip: true
                  wrapMode: Text.Wrap
               }

               CustomButton {
                  text: qsTr("Copy Address")
                  width: parent.width
                  height: BSSizes.applyScale(50)
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
            font.pixelSize: BSSizes.applyScale(14)
            color: SideSwapStyles.paragraphTextColor
         }

         Text {
            text: root.fromAddress
            font.pixelSize: BSSizes.applyScale(14)
            color: SideSwapStyles.paragraphTextColor
         }
      }

      CustomBorderedButton {
         text: qsTr("BACK")
         width: parent.width
         height: BSSizes.applyScale(60)
         anchors.horizontalCenter: parent.horizontalCenter
         onClicked: root.back()
      }
   }
}
