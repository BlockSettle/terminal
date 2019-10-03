import QtQuick 2.9
import QtQuick.Controls 2.3
import "../BsStyles"

Button {
    id: control
    text: parent.text
    leftPadding: 15
    rightPadding: 15
    anchors.margins: 5

    contentItem: Text {
        text: control.text
        opacity: enabled ? 1.0 : 0.3
        color: BSStyle.textColor
        font.capitalization: Font.AllUppercase
        font.pixelSize: 11
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        id: rect
        implicitWidth: 110
        implicitHeight: 35
        border.color: BSStyle.buttonsBorderColor
        color: BSStyle.buttonsPrimaryMainColor
        border.width: 0
    }

    states: [
        State {
            name: ""
            PropertyChanges {
                target: rect
                opacity: 1
            }
        },
        State {
            name: "pressed"
            when: control.pressed
            PropertyChanges {
                target: rect
                opacity: 0.7
            }
        },
        State {
            name: "hovered"
            when: control.hovered
            PropertyChanges {
                target: rect
                opacity: 0.85
            }
        },
        State {
            name: "disabled"
            when: !control.enabled
            PropertyChanges {
                target: rect
                opacity: 0.3
            }
        }
    ]

    transitions: [
        Transition {
            from: ""; to: "hovered"
            ColorAnimation { duration: 100 }
        },
        Transition {
            from: "*"; to: "pressed"
            ColorAnimation { duration: 10 }
        }
    ]

}

