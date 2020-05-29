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

MouseArea {
    hoverEnabled: true
    acceptedButtons: Qt.RightButton
    cursorShape: Qt.IBeamCursor
    onClicked: {
        if (mouse.button === Qt.RightButton) {
            let selectStart = root.selectionStart
            let selectEnd = root.selectionEnd
            let curPos = root.cursorPosition
            contextMenu.popup()
            root.cursorPosition = curPos
            root.select(selectStart,selectEnd)
        }
    }
    onPressAndHold: {
        if (mouse.source === Qt.MouseEventNotSynthesized) {
            let selectStart = root.selectionStart
            let selectEnd = root.selectionEnd
            let curPos = root.cursorPosition
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
