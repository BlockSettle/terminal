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

import com.blocksettle.QmlFactory 1.0

import "../BsStyles"

Label {
    id: root

    property alias mouseArea: mouseArea
    property string textForCopy

    font.pixelSize: 11
    color: "white"
    wrapMode: Text.WordWrap
    padding: 5
    onLinkActivated: Qt.openUrlExternally(link)

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true

        acceptedButtons: Qt.RightButton
        onClicked: {
            if (mouse.button === Qt.RightButton) {
                contextMenu.popup()
            }
        }
        onPressAndHold: {
            if (mouse.source === Qt.MouseEventNotSynthesized) {
                contextMenu.popup()
            }
        }

        Menu {
            id: contextMenu
            MenuItem {
                text: qsTr("Copy")
                onTriggered: {
                    qmlFactory.setClipboard(root.textForCopy.length > 0 ? root.textForCopy : root.text)
                }
            }
        }
    }
}

