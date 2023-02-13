/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.9
import QtQuick.Controls 2.3
import "../BsStyles"

Button {
    id: control
    property bool capitalize: true
    property bool primary: false
    text: parent.text
    leftPadding: 15
    rightPadding: 15
    anchors.margins: 5

    contentItem: Text {
        text: control.text
        opacity: enabled ? 1.0 : 0.3
        color: BSStyle.textColor
        font.capitalization: capitalize ? Font.AllUppercase : Font.MixedCase
        font.pixelSize: 11
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        id: rect
        implicitWidth: 110
        implicitHeight: 35
        opacity: primary ? 1 : (control.enabled ? 1 : 0.3)
        border.color: BSStyle.buttonsBorderColor
        color: primary ? BSStyle.buttonsPrimaryMainColor : (control.highlighted ? BSStyle.buttonsPrimaryMainColor : BSStyle.buttonsMainColor)
        border.width: primary ? 0 : 1
    }

    states: [
        State {
            name: ""
            PropertyChanges {
                target: rect
                opacity: primary ? 1 : (control.enabled ? 1 : 0.3)
                color: primary ? BSStyle.buttonsPrimaryMainColor : (control.highlighted ? BSStyle.buttonsPrimaryMainColor : BSStyle.buttonsMainColor)
            }
        },
        State {
            name: "pressed"
            when: control.pressed
            PropertyChanges {
                target: rect
                opacity: primary ? 0.7 : (control.enabled ? 1 : 0.3)
                color: primary ? BSStyle.buttonsPrimaryMainColor : (control.highlighted ? BSStyle.buttonsPrimaryPressedColor : BSStyle.buttonsPressedColor)
            }
        },
        State {
            name: "hovered"
            when: control.hovered
            PropertyChanges {
                target: rect
                opacity: primary ? 0.85 : (control.enabled ? 1 : 0.3)
                color: primary ? BSStyle.buttonsPrimaryMainColor : (control.highlighted ? BSStyle.buttonsPrimaryHoveredColor : BSStyle.buttonsHoveredColor)
            }
        },
        State {
            name: "disabled"
            when: !control.enabled
            PropertyChanges {
                target: rect
                opacity: primary ? 0.3 : (control.enabled ? 1 : 0.3)
                color: primary ? BSStyle.buttonsPrimaryMainColor : "gray"
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

