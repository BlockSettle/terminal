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
    property alias title_leftMargin: title.anchors.leftMargin
    property alias title_topMargin: title.anchors.topMargin
    property alias title_font_size: title.font.pixelSize

    //aliases - input
    property alias input_text: input.text
    property alias horizontalAlignment: input.horizontalAlignment
    property alias input_topMargin: input.anchors.topMargin

    signal textChanged()
    signal tabNavigated()
    signal backTabNavigated()


    color: "#020817"
    opacity: 1
    radius: 14

    border.color: input.activeFocus ? "#45A6FF" : BSStyle.defaultBorderColor
    border.width: 1

    Label {
        id: title

        anchors.top: rect.top
        anchors.topMargin: 16
        anchors.left: rect.left
        anchors.leftMargin: 16

        font.pixelSize: 13
        font.family: "Roboto"
        font.weight: Font.Normal

        color: "#7A88B0"
    }

    TextEdit {
        id: input

        focus: true
        activeFocusOnTab: true

        clip: true

        anchors.top: rect.top
        anchors.topMargin: 35
        anchors.left: rect.left
        anchors.leftMargin: title.anchors.leftMargin
        width: rect.width - 2*title.anchors.leftMargin
        height: 39

        font.pixelSize: 16
        font.family: "Roboto"
        font.weight: Font.Normal

        wrapMode : TextEdit.Wrap

        color: "#E2E7FF"

        onTextChanged : {
            rect.textChanged()

            var pos = input.positionAt(1, input.height + 1);
            if(input.length >= pos)
            {
                input.remove(pos, input.length);
            }
        }

        Keys.onTabPressed: {
            tabNavigated()
        }

        Keys.onBacktabPressed: {
            backTabNavigated()
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

    function setActiveFocus() {
        input.forceActiveFocus()
    }
}
