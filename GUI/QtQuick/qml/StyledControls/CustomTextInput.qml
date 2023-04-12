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

    //aliases - title
    property alias title_text: title.text
    property string placeholder_text
    property bool hide_placeholder_when_activefocus: true
    property int title_leftMargin:  BSSizes.applyScale(16)
    property int title_topMargin: BSSizes.applyScale(16)
    property int title_font_size: BSSizes.applyScale(13)

    //aliases - input
    property alias input_text: input.text
    property alias horizontalAlignment: input.horizontalAlignment
    property int input_topMargin: BSSizes.applyScale(35)
    property alias input_validator: input.validator

    property bool isValid: true
    property bool isPassword: false
    property bool isHiddenText: false

    property int input_right_margin: BSSizes.applyScale(16)

    property var completer: null

    signal textEdited()
    signal textChanged()
    signal editingFinished()
    signal activeFocusChanged()
    signal tabNavigated()
    signal backTabNavigated()
    signal enterPressed()
    signal returnPressed()


    color: "#020817"
    opacity: 1
    radius: BSSizes.applyScale(14)

    border.color: isValid ? (input.activeFocus ? "#45A6FF" : BSStyle.defaultBorderColor) : "#EB6060"
    border.width: 1

    Label {
        id: title

        anchors.top: rect.top
        anchors.topMargin: rect.title_topMargin
        anchors.left: rect.left
        anchors.leftMargin: rect.title_leftMargin

        font.pixelSize: rect.title_font_size
        font.family: "Roboto"
        font.weight: Font.Normal

        color: "#7A88B0"
    }

    TextInput {
        id: input

        focus: true
        activeFocusOnTab: true

        clip: true

        anchors.top: rect.top
        anchors.topMargin: rect.input_topMargin
        anchors.left: rect.left
        anchors.leftMargin: rect.title_leftMargin
        width: rect.width - rect.title_leftMargin - rect.input_right_margin
        height: BSSizes.applyScale(19)

        echoMode: isHiddenText? TextInput.Password : TextInput.Normal

        font.pixelSize: BSSizes.applyScale(16)
        font.family: "Roboto"
        font.weight: Font.Normal

        color: "#E2E7FF"

        Text {
            text: rect.placeholder_text
            color: "#7A88B0"
            visible: !input.text && (!hide_placeholder_when_activefocus || !input.activeFocus)

            font.pixelSize: BSSizes.applyScale(16)
            font.family: "Roboto"
            font.weight: Font.Normal
        }

        Keys.onEnterPressed: {
            enterPressed()
        }

        Keys.onReturnPressed: {
            returnPressed()
        }

        onTextEdited : {
            rect.textEdited()
        }

        onTextChanged : {
            rect.textChanged()
        }

        onEditingFinished : {
            rect.editingFinished()
        }

        onActiveFocusChanged: {
            rect.activeFocusChanged()
        }

        Keys.onTabPressed: {
            tabNavigated()
        }

        Keys.onBacktabPressed: {
            backTabNavigated()
        }
    }

    Image {
        id: eye_icon

        visible: isPassword

        anchors.top: rect.top
        anchors.topMargin: BSSizes.applyScale(23)
        anchors.right: rect.right
        anchors.rightMargin: BSSizes.applyScale(23)

        source: isHiddenText? "qrc:/images/Eye_icon _unvisible.png" : "qrc:/images/Eye_icon _visible.png"
        z: 1
        width: BSSizes.applyScale(24)
        height: BSSizes.applyScale(24)

        MouseArea {
            anchors.fill: parent
            onClicked: {
                isHiddenText = !isHiddenText
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            input.forceActiveFocus()
        }
    }

    function setActiveFocus() {
        input.forceActiveFocus()
    }
}
