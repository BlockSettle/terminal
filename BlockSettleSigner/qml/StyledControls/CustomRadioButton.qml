import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Controls.Styles 1.4
import "../BsStyles"

RadioButton {
    id: control
    text: parent.text

    indicator: Rectangle {
        implicitWidth: 22
        implicitHeight: 22
        x: control.leftPadding
        y: parent.height / 2 - height / 2
        radius: 11
        border.color: control.checked ? BSStyle.buttonsBorderColor : BSStyle.buttonsUncheckedColor
        color: "transparent"

        Rectangle {
            width: 8
            height: 8
            x: 7
            y: 7
            radius: 7
            color: control.checked ? BSStyle.buttonsPrimaryMainColor : BSStyle.buttonsUncheckedColor
            visible: control.checked
        }
    }

    contentItem: Text {
        text: control.text
        font: control.font
        opacity: enabled ? 1.0 : 0.3
        color: control.checked ? BSStyle.textColor : BSStyle.buttonsUncheckedColor
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + control.spacing
    }
}
