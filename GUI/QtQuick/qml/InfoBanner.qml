/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.9

Loader {
    id: messages
    property color bgColor

    function displayMessage(message) {
        messages.source = "";
        messages.source = Qt.resolvedUrl("InfoBannerComponent.qml");
        messages.item.message = message;
        messages.item.bgColor = bgColor
    }

    width: parent.width
    anchors.bottom: parent.top
    z: 5
    onLoaded: {
        messages.item.state = "portrait";
        timer.running = true
        messages.state = "show"
    }

    Timer {
        id: timer

        interval: 7000
        onTriggered: {
            messages.state = ""
        }
    }

    states: [
        State {
            name: "show"
            AnchorChanges { target: messages; anchors { bottom: undefined; top: parent.top } }
            PropertyChanges { target: messages; anchors.topMargin: 100 }
        }
    ]

    transitions: Transition {
        AnchorAnimation { easing.type: Easing.OutQuart; duration: 300 }
    }
}
