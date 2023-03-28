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
   property string inputCurrency
   property string outputCurrency: "BTC"
//   property string orderId: "253380c874f87a7a4f70"
   //property string conversionRate: "1 ETH = 0.06177451 BTC"
   //property string toAddress: "0x514cD3B3a164A78BA93b881C7b567d19CC6a1843"
   //property string networkFee: "1.97 USD"
   property string receivingAddress
   //property string creationDate: "2023-03-21 07:48"

   Rectangle {
      anchors.fill: parent
      color: "black"
   }

   Column {
      anchors.fill: parent
      anchors.leftMargin: 60
      anchors.rightMargin: 60
      anchors.topMargin: 25
      anchors.bottomMargin: 25
      spacing: 0

      Rectangle {
         color: "transparent"
         height: 40
         width: parent.width

         Row {
            spacing: 10
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter

            Text {
                //id: inputCurrency
                //model: controller.inputCurrencies
               color: "lightgray"
               font.weight: Font.Bold
               anchors.verticalCenter: parent.verticalCenter
            }

            Image {
               id: arrowImage
               width: 20
               height: 20
               source: "qrc:/sideshift_right_arrow.png"
               anchors.verticalCenter: parent.verticalCenter
            }

            Text {
               text: outputCurrency
               color: "lightgray"
               font.weight: Font.Bold
               anchors.verticalCenter: parent.verticalCenter
            }
         }

         Column {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter

            Text {
               color: "lightgray"
               text: qsTr("ORDER")
               anchors.right: parent.right
            }
            
            Text {
               color: "lightgray"
               text: controller.orderId
               font.weight: Font.Bold
            }
         }
      }

      Text {
         color: "white"
         text: qsTr("WAITING FOR YOU TO SEND ") + inputCurrency.currentText
         topPadding: 40
         font.pixelSize: 24
         anchors.horizontalCenter: parent.horizontalCenter
      }

      Text {
         topPadding: 15
         color: "lightgray"
         text: controller.conversionRate
         anchors.horizontalCenter: parent.horizontalCenter
      }

      Rectangle {
         width: parent.width
         height: 300
         color: "transparent"
         
         Column {
            spacing: 10
            width: parent.width / 2
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter

            Text {
               text: qsTr("PLEASE SEND")
               color: "lightgray"
               font.pixelSize: 20
               anchors.horizontalCenter: parent.horizontalCenter
            }

            Row {
               spacing: 10
               anchors.horizontalCenter: parent.horizontalCenter
               Text {
                  text: qsTr("Min")
                  color: "lightgray"
                  anchors.verticalCenter: parent.verticalCenter
               }
               Text {
                  text: controller.minAmount
                  color: "white"
                  font.weight: Font.Bold
                  font.pixelSize: 18
               }
               Text {
                  text: inputCurrency.currentText
                  color: "white"
                  font.weight: Font.Bold
                  font.pixelSize: 18
               }
            }

            Row {
               spacing: 10
               anchors.horizontalCenter: parent.horizontalCenter
               Text {
                  text: qsTr("Min")
                  color: "lightgray"
                  anchors.verticalCenter: parent.verticalCenter
               }
               Text {
                  text: controller.maxAmount
                  color: "white"
                  font.weight: Font.Bold
                  font.pixelSize: 18
               }
               Text {
                  text: outputCurrency
                  color: "white"
                  font.weight: Font.Bold
                  font.pixelSize: 18
               }
            }

            Text {
               topPadding: 20
               text: qsTr("TO ADDRESS")
               color: "lightgray"
               font.pixelSize: 20
               anchors.horizontalCenter: parent.horizontalCenter
            }

            Rectangle {
               height: 40
               width: 200
               color: "transparent"
               radius: 4
               border.width: 1
               border.color: "white"
               anchors.horizontalCenter: parent.horizontalCenter
            
               Text {
                  anchors.fill: parent
                  id: toAddress
                  text: controller.depositAddress
                  color: "white"
                  clip: true
                  font.weight: Font.Bold
                  verticalAlignment: Text.AlignVCenter
                  anchors.leftMargin: 20
                  anchors.rightMargin: 20
               }
            }

            SideShiftCopyButton {
               text: qsTr("COPY ADDRESS")
               anchors.horizontalCenter: parent.horizontalCenter
               onClicked: bsApp.copyAddressToClipboard(toAddress.text)
            }
         }

         Column {
            width: parent.width / 2
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter

            Rectangle {
               width: 200
               height: 200
               color: "gray"
               anchors.horizontalCenter: parent.horizontalCenter

               Image {
                   source: "image://QR/" + root.toAddress.text
                   sourceSize.width: 180
                   sourceSize.height: 180
                   width: 180
                   height: 180
                   anchors.centerIn: parent
               }
            }
         }
      }

      Item {
         width: 1
         height: 80
      }

      Text {
         text: qsTr("ESTIMATED NETWORK FEES: ") + controller.networkFee
         color: "white"
         anchors.horizontalCenter: parent.horizontalCenter
      }

      Item {
         width: 1
         height: 60
      }

      Rectangle {
         height: 40
         width: parent.width
         color: "transparent"

         Column {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter

            Text {
               color: "lightgray"
               text: qsTr("RECEIVING ADDRESS")
            }
            
            Text {
               width: 200
               color: "lightgray"
               text: receivingAddress
               id: receivingAddress
               font.weight: Font.Bold
               clip: true
            }
         }

         Column {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter

            Text {
               color: "lightgray"
               text: qsTr("CREATED AT")
               anchors.right: parent.right
            }
            
            Text {
               color: "lightgray"
               text: controller.creationDate
               font.weight: Font.Bold
               anchors.right: parent.right
            }
         }
      }
   }
}
