import QtQuick 2.15
import QtQuick.Controls 2.15

import "../BsStyles"

HorizontalHeaderView {
   delegate: Rectangle {

      implicitHeight: 34
      color: BSStyle.tableCellBackgroundColor

      Text {
         text: display
         height: parent.height
         verticalAlignment: Text.AlignVCenter
         clip: true
         color: BSStyle.titleTextColor
         font.family: "Roboto"
         font.weight: Font.Normal
         font.pixelSize: 11
         leftPadding: 10
      }

      Rectangle {
         height: 1
         width: parent.width
         color: BSStyle.tableSeparatorColor

         anchors.bottom: parent.bottom
      }
   }
}
