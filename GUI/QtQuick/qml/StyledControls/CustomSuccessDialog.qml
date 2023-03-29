import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"

Window  {
    id: root

    property alias details_text: success.details_text

    signal sig_finish()

    visible: true
    flags: Qt.WindowCloseButtonHint | Qt.FramelessWindowHint | Qt.Dialog
    modality: Qt.WindowModal

    height: 430
    width: 580

    color: "transparent"

    x: mainWindow.x + (mainWindow.width - width)/2
    y: mainWindow.y + (mainWindow.height - height)/2

    Rectangle {
        
      id: rect

      color: "#191E2A"
      opacity: 1
      radius: 16

      anchors.fill: parent

      border.color : BSStyle.defaultBorderColor
      border.width : 1

      CustomSuccessWidget {
        id: success

        anchors.topMargin: 24
        anchors.fill: parent
        details_font_size: 16
        details_font_weight: Font.Medium

        onSig_finish: {
          root.close()
          root.sig_finish()
        }
      }
    }
}