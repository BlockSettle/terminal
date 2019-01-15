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
        color: control.checked ? BSStyle.switchCheckedColor : (signerStatus.socketOk ? BSStyle.switchUncheckedColor : BSStyle.switchOrangeColor)
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }

    indicator: Rectangle {
        id: border_
        implicitWidth: 40
        implicitHeight: 20
        x: control.width - width - control.rightPadding
        y: parent.height / 2 - height / 2
        radius: 10
        color: control.checked ? BSStyle.switchCheckedColor : "transparent"
        border.color: control.checked ? BSStyle.switchCheckedColor : (signerStatus.socketOk ? BSStyle.switchUncheckedColor : BSStyle.switchOrangeColor)

        Rectangle {
            id: circle_
            x: control.checked ? parent.width - width : 0
            width: 20
            height: 20
            radius: 10
            color: control.checked ? BSStyle.textColor : (signerStatus.socketOk ? BSStyle.switchUncheckedColor : BSStyle.switchOrangeColor)
            border.color: control.checked ? (control.down ? BSStyle.switchCheckedColor : BSStyle.switchCheckedColor) : BSStyle.backgroundColor
        }
    }

    background: Rectangle {
        implicitWidth: 80
        implicitHeight: 20
        visible: control.down
        color: BSStyle.switchBgColor
    }

//    states: [
//        State {
//            name: "checked"
//            when: control.checked
//        },
//        State {
//            name: "uncheked"
//            when: !control.checked
//        }
//    ]

//    transitions: [
//        Transition {
//            to: "checked"
//            NumberAnimation {
//                target: circle_
//                properties: "color"
//                duration: 300
//            }
//        }
//    ]
}
