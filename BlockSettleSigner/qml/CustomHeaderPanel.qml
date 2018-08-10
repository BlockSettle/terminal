import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Controls.Styles 1.4

Button {
    leftPadding: 10
    rightPadding: 10
    width: parent.width
    height: parent.height



    background: Rectangle {
        color: "#0A1619"
        width: parent.width
        height: parent.height
    }

    contentItem: Text {
        height: parent.height
        text:  parent.text
        font.capitalization: Font.AllUppercase
        color: "#ffffff"
        font.pixelSize: 11
        verticalAlignment: Text.AlignVCenter
    }

}
