/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.12
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.3
import "../BsStyles"

TabButton {
    id: control

    width: 94
    height: parent.height

    focusPolicy: Qt.NoFocus

    font.pixelSize: 10
    font.family: "Roboto"
    font.weight: Font.Medium

    property url selectedIcon_;
    property url nonSelectedIcon_;

    contentItem: ColumnLayout {
        width: control.width
        height: control.height
        spacing : 4

        Image {
            id: image_

            width: 24
            height: 24
            Layout.alignment : Qt.AlignTop | Qt.AlignHCenter

            source: control.checked? selectedIcon_ :nonSelectedIcon_
            sourceSize: Qt.size(parent.width, parent.height)

            smooth: true
        }

        Text {
            id: text_

            Layout.alignment : Qt.AlignBottom | Qt.AlignHCenter

            text: control.text
            font: control.font
            color: !control.enabled ? BSStyle.disabledTextColor :
                              (control.down ? BSStyle.textPressedColor :
                              (control.checked? BSStyle.selectedColor : BSStyle.titleTextColor))
        }
    }


    background: Rectangle {
        implicitWidth: parent.width
        implicitHeight: parent.height
        color: "transparent"
    }

    function setIcons(selectedIcon: url, nonselectedIcon: url)
    {
        selectedIcon_ = selectedIcon;
        nonSelectedIcon_ = nonselectedIcon;
    }
}

