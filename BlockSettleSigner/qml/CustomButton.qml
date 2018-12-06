import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Controls.Styles 1.4

Button {
    id: control
    property bool capitalize: true
    text: parent.text
    leftPadding: 15
    rightPadding: 15

    contentItem: Text {
        text: control.text
        opacity: enabled ? 1.0 : 0.3
        color: "#ffffff"
        font.capitalization: capitalize ? Font.AllUppercase : Font.MixedCase
        font.pixelSize: 11
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        implicitWidth: 110
        implicitHeight: 35
        opacity: enabled ? 1 : 0.3
        border.color: "#247dac"
        color: control.down || control.checked ? "#247dac" : "transparent"

        border.width: 1
    }
}

