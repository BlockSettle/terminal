import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Controls.Styles 1.4

TabButton {
    id: control
    text: parent.text

    contentItem: Text {
        text: control.text
        font.capitalization: Font.AllUppercase
        font.pixelSize: 12
        color: control.checked ? (control.down ? "#3a8ab4" : "#ffffff") : "#81888f"
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        implicitWidth: 100
        implicitHeight: 50
        opacity: enabled ? 1 : 0.3
        color: control.checked ? (control.down ? "#1C2835" : "#1C2835") : "0f1f24"

        Rectangle {
            width: parent.width
            height: 2
            color: control.checked ? (control.down ? "#3a8ab4" : "#247dac") : "transparent"
            anchors.top: parent.top
        }
    }
}

