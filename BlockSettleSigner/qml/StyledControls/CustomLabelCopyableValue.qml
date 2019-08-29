import QtQuick 2.9
import QtQuick.Controls 2.3

import com.blocksettle.QmlFactory 1.0

import "../BsStyles"

Label {
    id: root

    property alias mouseArea: mouseArea
    property string textForCopy

    font.pixelSize: 12
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

