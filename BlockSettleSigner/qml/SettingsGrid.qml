/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

GridLayout {
    id: grid
    columns: 2
    columnSpacing: 5
    rowSpacing: 5
    Layout.fillWidth: true
    Layout.fillHeight: true
    Layout.minimumHeight: 25
    Layout.leftMargin: 10
    Layout.rightMargin: 10
    state: "normal"
    property real originalHeight: 0


    Component.onCompleted: {
        originalHeight = height
    }

    states: [
        State {
            name: "normal"
            PropertyChanges { target: grid; scale: 1.0 }
            PropertyChanges { target: grid; implicitHeight: originalHeight }
            PropertyChanges { target: grid; enabled: true }
        },
        State {
            name: "hidden"
            PropertyChanges { target: grid; scale: 0.00 }
            PropertyChanges { target: grid; implicitHeight: 0.00 * originalHeight }
            PropertyChanges { target: grid; enabled: false }
        }
    ]
    transitions: [
        Transition {
            to: "hidden"
            NumberAnimation {
                target: grid
                properties: "scale,implicitHeight"
                duration: 400
                easing.type: Easing.InQuad
            }
        },
        Transition {
            to: "normal"
            NumberAnimation {
                target: grid
                properties: "scale,implicitHeight"
                duration: 400
                easing.type: Easing.InQuad
            }
        }
    ]
}
