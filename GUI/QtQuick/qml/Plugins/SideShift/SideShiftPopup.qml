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

PluginPopup {
   id: root

   property bool receive: true
   property var receiveModel: [
        { 
            currency: "BTC",
            icon: "qrc:/images/sideshift_btc.png"
        },
        { 
            currency: "ETC",
            icon: "qrc:/images/sideshift_btc.png"
        },
        { 
            currency: "DOG",
            icon: "qrc:/images/sideshift_btc.png"
        },
        { 
            currency: "XYZ",
            icon: "qrc:/images/sideshift_btc.png"
        },
    ]
   property var sendModel: [
        { 
            currency: "BTC",
            icon: "qrc:/images/sideshift_btc.png"
        }
    ]

   background: Rectangle {
      anchors.fill: parent
      color: "black"
      radius: 14
   }

   contentItem: StackView {
      id: stackView
      initialItem: mainPage
      anchors.fill: parent
   }

   Component {
      id: mainPage

      SideShiftMainPage {

      }
   }

   function reset() {
      stackView.clear()
      stackView.pop()
   }
}
