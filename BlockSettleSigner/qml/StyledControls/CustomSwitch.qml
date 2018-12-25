import QtQuick 2.9
import QtQuick.Controls 2.3
import "../BsStyles"

Switch {
    id: control
    text: parent.text
    checked: true

    contentItem: Text {
        rightPadding: control.indicator.width + control.spacing
        text: control.text
        font: control.font
        opacity: enabled ? 1.0 : 0.3
        color: control.checked ? BSStyle.switchGreenColor : (signerStatus.socketOk ? BSStyle.switchRedColor : BSStyle.switchOrangeColor)
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }

    indicator: Rectangle {
        implicitWidth: 48
        implicitHeight: 26
        x: control.width - width - control.rightPadding
        y: parent.height / 2 - height / 2
        radius: 13
        color: control.checked ? BSStyle.switchGreenColor : "transparent"
        border.color: control.checked ? BSStyle.switchGreenColor : (signerStatus.socketOk ? BSStyle.switchRedColor : BSStyle.switchOrangeColor)

        Rectangle {
            x: control.checked ? parent.width - width : 0
            width: 26
            height: 26
            radius: 13
            color: control.checked ? BSStyle.textColor : (signerStatus.socketOk ? BSStyle.switchRedColor : BSStyle.switchOrangeColor)
            border.color: control.checked ? (control.down ? BSStyle.switchGreenColor : BSStyle.switchGreenColor) : BSStyle.backgroundColor
        }
    }

    background: Rectangle {
        implicitWidth: 100
        implicitHeight: 26
        visible:  control.down
        color: BSStyle.switchBgColor
    }
}
