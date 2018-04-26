import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Controls.Styles 1.4

CheckBox {
    id: control
    text: parent.text

    indicator: Rectangle {
        implicitWidth: 25
        implicitHeight: 25
        y: parent.height / 2 - height / 2
        radius: 0
        border.color: control.checked ? "#247dac" : "#81888f"
        color: "transparent"

        Rectangle {
            width: 9
            height: 9
            x: 8
            y: 8
            radius: 0
            color: control.checked ? "#247dac" : "#81888f"
            visible: control.checked
        }
    }

    contentItem: Text {
        text: control.text
        font.pixelSize: 11
        opacity: enabled ? 1.0 : 0.3
        color: control.checked ? "#ffffff" : "#81888f"
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + control.spacing
    }
}
