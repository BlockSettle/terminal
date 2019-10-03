import QtQuick 2.9
import QtQuick.Controls 2.3
import "../BsStyles"

Button {
    leftPadding: 0
    focusPolicy: Qt.NoFocus

    property color textColor: BSStyle.textColor
    background: Rectangle {
        color: "transparent"
    }

    contentItem: Text {
        text: parent.text
        font.capitalization: Font.AllUppercase
        color: { parent.enabled ? textColor : BSStyle.disabledHeaderColor }
        font.pixelSize: 11
    }

    Rectangle {
        height: 1
        width: parent.width
        color: Qt.rgba(1, 1, 1, 0.1)
        anchors.bottom: parent.bottom
    }
}
