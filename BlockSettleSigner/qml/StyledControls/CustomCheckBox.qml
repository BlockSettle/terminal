import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Controls.Styles 1.4
import "../BsStyles"

CheckBox {
    id: control
    text: parent.text

    indicator: Rectangle {
        implicitWidth: 25
        implicitHeight: 25
        y: parent.height / 2 - height / 2
        radius: 0
        border.color: control.checked ? BSStyle.buttonsBorderColor : BSStyle.buttonsUncheckedColor
        color: "transparent"

        Rectangle {
            width: 9
            height: 9
            x: 8
            y: 8
            radius: 0
            color: control.checked ? BSStyle.buttonsPrimaryMainColor : BSStyle.buttonsUncheckedColor
            visible: control.checked
        }
    }

    contentItem: Text {
        text: control.text
        font.pixelSize: 11
        opacity: enabled ? 1.0 : 0.3
        color: control.checked ? BSStyle.textColor : BSStyle.buttonsUncheckedColor
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + control.spacing
    }
}
