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

    property alias title_text: title.text
    property alias input_text: input.text
    property alias horizontalAlignment: input.horizontalAlignment
    property alias input_topMargin: input.anchors.topMargin
    property alias title_leftMargin: title.anchors.leftMargin
    property alias title_topMargin: title.anchors.topMargin

    property alias input_item: input

    property bool isValid: true
    property bool isPassword: false
    property bool isHiddenText: false

    signal textChanged()


    color: "#020817"
    opacity: 1
    radius: 14

    border.color: isValid ? (input.focus ? "#45A6FF" : "#3C435A") : "#EB6060"
    border.width: 1

    Label {
        id: title

        anchors.top: rect.top
        anchors.left: rect.left

        font.pixelSize: 12
        font.family: "Roboto"
        font.weight: Font.Normal

        color: "#7A88B0"
    }

    TextInput {
        id: input

        anchors.top: rect.top
        //anchors.topMargin: (rect.height - 19)/2
        anchors.left: rect.left
        anchors.leftMargin: title.anchors.leftMargin
        width: rect.width - 2*title.anchors.leftMargin
        height: 19

        echoMode: isHiddenText? TextInput.Password : TextInput.Normal
        //placeholderText: isPassword? qsTr("Password") : ""
        //placeholderTextColor: "#32394F"

        font.pixelSize: 16
        font.family: "Roboto"
        font.weight: Font.Normal

        color: "#E2E7FF"

        onTextChanged : {
            rect.textChanged()
        }
    }


    Image {
        id: eye_icon

        visible: isPassword

        anchors.top: rect.top
        anchors.topMargin: 23
        anchors.right: rect.right
        anchors.rightMargin: 23

        source: isHiddenText? "qrc:/images/Eye_icon _unvisible.png" : "qrc:/images/Eye_icon _visible.png"
        width: 24
        height: 24

        MouseArea {
            anchors.fill: parent
            onClicked: {
                isHiddenText = !isHiddenText
                console.log("isHiddenText = " + isHiddenText)
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        propagateComposedEvents: true
        onClicked: {
            input.forceActiveFocus()
            mouse.accepted = false
        }
    }
}
