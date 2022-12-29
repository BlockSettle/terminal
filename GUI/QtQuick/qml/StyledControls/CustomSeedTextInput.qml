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

Rectangle {
    id: rect

    property alias serial_num: serial_number.text
    property alias seed_text: seed.text
    property bool isValid: false

    signal textChanged()

    height: 46

    color: "#020817"
    opacity: 1
    radius: 14

    border.color: isValid ? (seed.focus ? "#45A6FF" : "#3C435A") : "#EB6060"
    border.width: 1

    Label {
        id: serial_number

        anchors.top: rect.top
        anchors.topMargin: 8
        anchors.left: rect.left
        anchors.leftMargin: 10

        font.pixelSize: 12
        font.family: "Roboto"
        font.weight: Font.Normal

        color: "#7A88B0"
    }

    TextInput {
        id: seed

        anchors.top: rect.top
        anchors.topMargin: 13
        anchors.bottom: rect.bottom
        anchors.bottomMargin: 13
        anchors.left: rect.left
        anchors.leftMargin: 50
        width: rect.width - 100

        horizontalAlignment : TextInput.AlignHCenter

        font.pixelSize: 16
        font.family: "Roboto"
        font.weight: Font.Normal

        color: "#E2E7FF"

        onTextChanged : {
            rect.textChanged()
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            seed.forceActiveFocus()
        }
    }
}
