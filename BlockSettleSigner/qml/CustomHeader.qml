import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Controls.Styles 1.4

Button {
    leftPadding: 0


        background: Rectangle {
            color: "transparent"
        }

        contentItem: Text {
            text:  parent.text
            font.capitalization: Font.AllUppercase
            color: "#ffffff"
            font.pixelSize: 11

        }

        Rectangle {
            height: 1
            width: parent.width
            color: Qt.rgba(1, 1, 1, 0.1)
            anchors.bottom: parent.bottom

        }


}
