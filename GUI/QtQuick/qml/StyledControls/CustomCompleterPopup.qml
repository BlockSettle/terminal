/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"


Popup {

    id: _popup

    property var comp_vars
    property bool not_valid_word: false
    property alias current_index: comp_list.currentIndex

    padding: 6
    focus: false

    contentItem: ListView {
        id: comp_list

        clip: true
        implicitHeight: contentHeight
        model: comp_vars

        delegate: ItemDelegate {

            id: delega

            width: _popup.width - 12
            height: 27

            leftPadding: 6
            topPadding: 4
            bottomPadding: 4

            contentItem: Text {

                text: comp_vars[index]
                color: delega.highlighted ? BSStyle.comboBoxItemTextHighlightedColor :
                                            BSStyle.comboBoxItemTextColor
                font.pixelSize: 16
                font.family: "Roboto"
                font.weight: Font.Normal

                elide: Text.ElideNone
                verticalAlignment: Text.AlignVCenter
            }

            highlighted: comp_list.currentIndex === index && !_popup.not_valid_word

            background: Rectangle {
                color: delega.highlighted ? BSStyle.comboBoxItemHighlightedColor : "transparent"
                opacity: delega.highlighted ? 0.2 : 1
                radius: 14
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    comp_list.currentIndex = index
                }
            }
        }
    }

    background: Rectangle {
        color: "#FFFFFF"
        radius: 14
    }

    function current_increment ()
    {
        if (_popup.visible)
        {
            comp_list.currentIndex = comp_list.currentIndex + 1
            if (comp_list.currentIndex >= comp_list.count)
                comp_list.currentIndex = 0
        }
    }

    function current_decrement ()
    {
        if (_popup.visible)
        {
            comp_list.currentIndex = comp_list.currentIndex - 1
            if (comp_list.currentIndex < 0)
                comp_list.currentIndex = comp_list.count - 1
        }
    }
}
