import QtQuick 2.9
import QtQuick.Controls 2.3
import "../BsStyles"


TextField {
    horizontalAlignment: Text.AlignHLeft
    font.pixelSize: 12
    color: BSStyle.inputsFontColor
    padding: 0
    echoMode: button.pressed ? TextInput.Normal : TextInput.Password
    selectByMouse: false

    background: Rectangle {
        implicitWidth: 200
        implicitHeight: 25
        color:"transparent"
        border.color: BSStyle.inputsBorderColor

        Button {
            id: button
//            contentItem: Text {
//                text: "👁"
//                color: BSStyle.textColor
//                font.pixelSize: 12 + (button.pressed ? 1 : 0)
//                horizontalAlignment: Text.AlignHCenter
//                verticalAlignment: Text.AlignVCenter
//            }
            contentItem: Rectangle {
                color: "transparent"
                Image {
                    fillMode: Image.PreserveAspectFit
                    anchors.fill: parent
                    source: "qrc:/resources/eye.png"
                }
            }
            padding: 2 - (button.pressed ? 1 : 0)
            background: Rectangle {color: "transparent"}
            anchors.right: parent.right
            width: 23
            height: 23
        }
    }
}
