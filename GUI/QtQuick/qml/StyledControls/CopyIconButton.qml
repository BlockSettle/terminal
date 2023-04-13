
import QtQuick 2.15
import QtQuick.Controls 2.15

import "../BsStyles"

Image {
    id: control

    signal copy();

    width: BSSizes.applyScale(24)
    height: BSSizes.applyScale(24)
    anchors.verticalCenter: parent.verticalCenter
    source: "qrc:/images/copy_icon.svg"
    MouseArea {
        anchors.fill: parent
        ToolTip {
            id: tool_tip
            timeout: 1000
            text: qsTr("Copied")
            font.pixelSize: BSSizes.applyScale(10)
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
                border.width: BSSizes.applyScale(1)
                radius: BSSizes.applyScale(14)
            }
        }
        onClicked: {
            control.copy()
            tool_tip.visible = true
        }
    }
}
