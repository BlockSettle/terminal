/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.9
import QtQuick.Controls 2.3

import "../BsStyles"

TextField {
    id: root
    horizontalAlignment: Text.AlignHLeft
    font.pixelSize: 11
    color: BSStyle.inputsFontColor
    padding: 0
    selectByMouse: true

    property int selectStart
    property int selectEnd
    property int curPos

    background: Rectangle {
        implicitWidth: 200
        implicitHeight: 25
        color: "transparent"
        border.color: BSStyle.inputsBorderColor
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.RightButton
        cursorShape: Qt.IBeamCursor
        onClicked: {
            if (mouse.button === Qt.RightButton) {
                selectStart = root.selectionStart
                selectEnd = root.selectionEnd
                curPos = root.cursorPosition
                contextMenu.popup()
                root.cursorPosition = curPos
                root.select(selectStart,selectEnd)
            }
        }
        onPressAndHold: {
            if (mouse.source === Qt.MouseEventNotSynthesized) {
                selectStart = root.selectionStart
                selectEnd = root.selectionEnd
                curPos = root.cursorPosition
                contextMenu.popup()
                root.cursorPosition = curPos
                root.select(selectStart,selectEnd)
            }
        }

        Menu {
            id: contextMenu
            MenuItem {
                text: qsTr("Cut")
                onTriggered: {
                    root.cut()
                }
            }
            MenuItem {
                text: qsTr("Copy")
                onTriggered: {
                    root.copy()
                }
            }
            MenuItem {
                text: qsTr("Paste")
                onTriggered: {
                    root.paste()
                }
            }
        }
    }
}
