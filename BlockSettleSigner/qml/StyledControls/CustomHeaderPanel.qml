import QtQuick 2.9
import QtQuick.Controls 2.3
import "../BsStyles"

Button {
    leftPadding: 10
    rightPadding: 10

    background: Rectangle {
        color: BSStyle.dialogHeaderColor
        width: parent.width
        height: parent.height
    }

    contentItem: Text {
        height: parent.height
        text: parent.text
        font.capitalization: Font.AllUppercase
        color: BSStyle.textColor
        font.pixelSize: 12
        verticalAlignment: Text.AlignVCenter
    }
}
