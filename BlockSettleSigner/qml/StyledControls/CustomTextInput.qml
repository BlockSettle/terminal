import QtQuick 2.9
import QtQuick.Controls 2.3
import "../BsStyles"

TextField {
    horizontalAlignment: Text.AlignHLeft
    font.pixelSize: 12
    color: BSStyle.inputsFontColor
    padding: 0
    selectByMouse: true

    background: Rectangle {
        implicitWidth: 200
        implicitHeight: 25
        color:"transparent"
        border.color: BSStyle.inputsBorderColor
    }
}
