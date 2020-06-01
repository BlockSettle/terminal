/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.9

Item {
    id: banner

    property alias message: messageText.text
    property alias bgColor: background.color

    height: 70

    Rectangle {
        id: background

        anchors.fill: banner
        smooth: true
        opacity: 0.8
    }

    Text {
        font.pixelSize: 20
        renderType: Text.QtRendering
        width: 150
        height: 40
        id: messageText


        anchors.fill: banner
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.WordWrap
        color: "white"
    }

    states: State {
        name: "portrait"
        PropertyChanges { target: banner; height: 100 }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            messageText.state = ""
        }
    }
}
