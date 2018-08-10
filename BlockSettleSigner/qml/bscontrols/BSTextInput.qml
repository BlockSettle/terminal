import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Controls.Styles 1.4

TextField {
    horizontalAlignment: Text.AlignHLeft
    font.pixelSize: 11
    color: BSStyle.inputsFontColor

    background: Rectangle {
        implicitWidth: 200
        implicitHeight: 25
        color:"transparent"
        border.color: BSStyle.inputsBorderColor
    }
}
