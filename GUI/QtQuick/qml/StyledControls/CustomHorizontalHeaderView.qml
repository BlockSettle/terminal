import QtQuick 2.15
import QtQuick.Controls 2.15

import "../BsStyles"

HorizontalHeaderView {
   id: root
   property int text_size

   delegate: Rectangle {

      implicitHeight: BSSizes.applyScale(34)
      implicitWidth: BSSizes.applyScale(100)
      color: BSStyle.tableCellBackgroundColor

      Text {
         text: display
         height: parent.height
         verticalAlignment: Text.AlignVCenter
         clip: true
         color: BSStyle.titleTextColor
         font.family: "Roboto"
         font.pixelSize: root.text_size
         font.letterSpacing: -0.2
         leftPadding: BSSizes.applyScale(10)
      }

      Rectangle {
         height: BSSizes.applyScale(1)
         width: parent.width
         color: BSStyle.tableSeparatorColor

         anchors.bottom: parent.bottom
      }
   }
}
