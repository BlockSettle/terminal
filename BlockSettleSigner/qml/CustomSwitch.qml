import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Controls.Styles 1.4

Switch {
    id: control
    text: parent.text
    checked: true

    contentItem: Text {
        rightPadding: control.indicator.width + control.spacing
        text: control.text
        font: control.font
        opacity: enabled ? 1.0 : 0.3
        color: control.checked ? "#22C064" : (signerStatus.socketOk ? "#EC0A35" : "#f6a724")
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }

    indicator: Rectangle {
        implicitWidth: 48
        implicitHeight: 26
        x: control.width - width - control.rightPadding
        y: parent.height / 2 - height / 2
        radius: 13
        color: control.checked ? "#22C064" : "transparent"
        border.color: control.checked ? "#22C064" : (signerStatus.socketOk ? "#EC0A35" : "#f6a724")

        Rectangle {
            x: control.checked ? parent.width - width : 0
            width: 26
            height: 26
            radius: 13
            color: control.checked ? "#ffffff" : (signerStatus.socketOk ? "#EC0A35" : "#f6a724")
            border.color: control.checked ? (control.down ? "#22C064" : "#38C673") : "#1c2835"
        }
    }

    background: Rectangle {
        implicitWidth: 100
        implicitHeight: 40
        visible:  control.down
        color: "transparent"
    }
}
