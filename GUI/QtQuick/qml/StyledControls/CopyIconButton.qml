
import QtQuick 2.15
import QtQuick.Controls 2.15

import "../BsStyles"

Image {
    id: control

    signal copy();

    width: 24
    height: 24
    anchors.verticalCenter: parent.verticalCenter
    source: "qrc:/images/copy_icon.svg"
    MouseArea {
        anchors.fill: parent
        ToolTip {
            id: tool_tip
            timeout: 1000
            text: qsTr("Copied")
            font.pixelSize: 10
            font.family: "Roboto"
            font.weight: Font.Normal
            contentItem: Text {
                text: tool_tip.text
                font: tool_tip.font
                color: BSStyle.textColor
            }
            background: Rectangle {
                color: BSStyle.buttonsStandardColor
                border.color: BSStyle.buttonsStandardColor
                border.width: 1
                radius: 14
            }
        }
        onClicked: {
            control.copy()
            tool_tip.visible = true
        }
    }
}
