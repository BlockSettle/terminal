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
   property string outputCurrency
   property string receivingAddress

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
               text: root.inputCurrency
               color: "lightgray"
               font.weight: Font.Bold
               anchors.verticalCenter: parent.verticalCenter
            }

            Image {
               id: arrowImage
               width: 20
               height: 12
               sourceSize.width: 20
               sourceSize.height: 12
               source: "qrc:/images/sideshift_right_arrow.svg"
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
               linkColor: "lightgray"
               font.weight: Font.Bold
               text: "<a href=\"https://sideshift.ai/orders/%1\">%1</a>".arg(controller !== null ? controller.orderId : "")
               onLinkActivated: Qt.openUrlExternally("https://sideshift.ai/orders/%1".arg(controller !== null ? controller.orderId : ""))
            }
         }
      }

      Text {
         color: "white"
         text: controller !== null ? controller.status : ""
         topPadding: 40
         wrapMode: Text.Wrap
         font.pixelSize: 24
         anchors.horizontalCenter: parent.horizontalCenter
         width: parent.width
      }

      Text {
         topPadding: 15
         color: "lightgray"
         text: controller !== null ? controller.conversionRate : ""
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
                  text: controller !== null ? controller.minAmount : ""
                  color: "white"
                  font.weight: Font.Bold
                  font.pixelSize: 18
               }
               Text {
                  text: root.inputCurrency
                  color: "white"
                  font.weight: Font.Bold
                  font.pixelSize: 18
               }
            }

            Row {
               spacing: 10
               anchors.horizontalCenter: parent.horizontalCenter
               Text {
                  text: qsTr("Max")
                  color: "lightgray"
                  anchors.verticalCenter: parent.verticalCenter
               }
               Text {
                  text: controller !== null ? controller.maxAmount : ""
                  color: "white"
                  font.weight: Font.Bold
                  font.pixelSize: 18
               }
               Text {
                  text: root.inputCurrency
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
            
               TextInput {
                  id: toAddress
                  anchors.fill: parent
                  text: controller !== null ? controller.depositAddress : ""
                  color: "white"
                  clip: true
                  font.weight: Font.Bold
                  verticalAlignment: Text.AlignVCenter
                  anchors.leftMargin: 20
                  anchors.rightMargin: 20
                  enabled: false
               }
            }

            SideShiftCopyButton {
               text: timer.running ? qsTr("COPIED") : qsTr("COPY ADDRESS")
               anchors.horizontalCenter: parent.horizontalCenter
               onClicked: {
                  toAddress.selectAll()
                  bsApp.copyAddressToClipboard(toAddress.text)
                  timer.start()
               }

               Timer {
                   id: timer
                   repeat: false
                   interval: 5000
                   onTriggered: toAddress.select(0, 0)
               }
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
                   source: controller !== null ? ( "image://QR/" + controller.depositAddress) : ""
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
         text: qsTr("ESTIMATED NETWORK FEES: ") + (controller !== null ? controller.networkFee : "")
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
               text: root.receivingAddress
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
               text: controller !== null ? controller.creationDate : ""
               font.weight: Font.Bold
               anchors.right: parent.right
            }
         }
      }
   }

   Timer {
      id: updateTimer
      interval: 5000
      repeat: true
      onTriggered: {
          controller.updateShiftStatus()
          if (controller.status === "complete") {
              close()
          }
      }
   }

   onVisibleChanged: {
      if (visible) {
         updateTimer.start()
      }
      else {
         updateTimer.stop()
      }
   }
}
