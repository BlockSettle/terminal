/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.15
import QtQuick.Controls 2.3


Button {
  id: control

  width: 150
  height: 50

  font.pixelSize: 14
  font.family: "Roboto"
  font.weight: Font.Bold

  hoverEnabled: true

  background: Rectangle {
    id: backgroundItem
    color: "transparent"
  }

  contentItem: Row {
    spacing: 5
    anchors.fill: parent

    Image {
      width: 20
      height: 20
      source: "qrc:/images/copy_icon.svg"
    }

    Text {
      text: control.text
      font: control.font
      color: "skyblue"
      verticalAlignment: Text.AlignVCenter
      horizontalAlignment: Text.AlignHCenter
    }
  }
}
