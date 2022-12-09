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
import QtQuick.Layouts 1.3
import "../BsStyles"

TabButton {
    id: control
    text: parent.text
    icon.source: parent.icon.source
    property alias cText: text_
    property alias cIcon: image_
    focusPolicy: Qt.NoFocus

    contentItem: RowLayout {
        width: parent.width
        height: parent.height
    
    Image {
        id: image_
        source: control.icon.source
        horizontalAlignment: Qt.AlignVCenter
        verticalAlignment: Qt.AlignVCenter
        Layout.fillHeight: true
    }

    Text {
        id: text_
        text: control.text
        font.capitalization: Font.AllUppercase
        font.pointSize: 10
        color: control.checked ? (control.down ? BSStyle.textPressedColor : BSStyle.textColor) : BSStyle.buttonsUncheckedColor
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignLeft
        verticalAlignment: Text.AlignVCenter
    }
    }

    background: Rectangle {
        implicitWidth: 100
        implicitHeight: 50
        opacity: enabled ? 1 : 0.3
        color: control.checked ? (control.down ? BSStyle.backgroundPressedColor : BSStyle.backgroundColor) : "0f1f24"

        Rectangle {
            width: parent.width
            height: 2
            color: control.checked ? (control.down ? BSStyle.textPressedColor : BSStyle.buttonsPrimaryMainColor) : "transparent"
            anchors.top: parent.top
        }
    }
}

