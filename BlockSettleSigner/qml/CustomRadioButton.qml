import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Controls.Styles 1.4

RadioButton {
    id: control
    text: parent.text

    indicator: Rectangle {
        implicitWidth: 22
        implicitHeight: 22
        x: control.leftPadding
        y: parent.height / 2 - height / 2
        radius: 11
        border.color: control.checked ? "#247dac" : "#81888f"
        color: "transparent"

        Rectangle {
            width: 8
            height: 8
            x: 7
            y: 7
            radius: 7
            color: control.checked ? "#247dac" : "#81888f"
            visible: control.checked
        }
    }

    contentItem: Text {
        text: control.text
        font: control.font
        opacity: enabled ? 1.0 : 0.3
        color: control.checked ? "#ffffff" : "#81888f"
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + control.spacing
    }
}
