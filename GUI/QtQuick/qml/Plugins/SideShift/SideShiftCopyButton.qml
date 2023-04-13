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
import "../../BsStyles"


Button {
  id: control

  width: BSSizes.applyScale(150)
  height: BSSizes.applyScale(50)

  font.pixelSize: BSSizes.applyScale(14)
  font.family: "Roboto"
  font.weight: Font.Bold

  hoverEnabled: true

  background: Rectangle {
    id: backgroundItem
    color: "transparent"
  }

  contentItem: Row {
    spacing: BSSizes.applyScale(5)
    anchors.fill: parent

    Image {
      width: BSSizes.applyScale(20)
      height: BSSizes.applyScale(20)
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
