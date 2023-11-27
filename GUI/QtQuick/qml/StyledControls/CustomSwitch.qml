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
        implicitWidth: BSSizes.applyScale(40)
        implicitHeight: BSSizes.applyScale(20)
        x: control.width - width - control.rightPadding
        y: parent.height / 2 - height / 2
        radius: BSSizes.applyScale(10)
        color: {
            if (control.enabled) {
                if (control.checked) {
                    return BSStyle.switchCheckedColor
                }
                else {
                    return BSStyle.switchBgColor
                }
            }
            else {
                return BSStyle.switchDisabledBgColor

            }
        }
        border.color: {
            if (control.enabled) {
                if (control.checked) {
                    return BSStyle.switchCheckedColor
                }
                else {
                    return BSStyle.switchUncheckedColor
                }
            }
            else {
                return BSStyle.switchDisabledBgColor
            }
        }
        Rectangle {
            id: circle_
            x: control.checked ? parent.width - width : 0
            width: BSSizes.applyScale(20)
            height: BSSizes.applyScale(20)
            radius: BSSizes.applyScale(10)

            color: {
                if (control.enabled) {
                    if (control.checked) {
                        return BSStyle.textColor
                    }
                    else {
                        return BSStyle.switchUncheckedColor
                    }
                }
                else {
                    return "#71787f"

                }
            }

            border.color: {
                if (control.enabled) {
                    if (control.checked) {
                        return BSStyle.switchCheckedColor
                    }
                    else {
                        return BSStyle.backgroundColor
                    }
                }
                else {
                    return BSStyle.switchDisabledBgColor
                }
            }
        }
    }

    background: Rectangle {
        implicitWidth: BSSizes.applyScale(80)
        implicitHeight: BSSizes.applyScale(20)
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
