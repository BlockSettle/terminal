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
    property int index
    property alias current_index: comp_list.currentIndex

    signal compChoosed ()

    padding: 6

    contentItem: ListView {
        id: comp_list

        clip: true
        implicitHeight: contentHeight
        model: comp_vars

        delegate: ItemDelegate {

            id: delega

            width: _popup.width - BSSizes.applyScale(12)
            height: BSSizes.applyScale(27)

            leftPadding: BSSizes.applyScale(6)
            topPadding: BSSizes.applyScale(4)
            bottomPadding: BSSizes.applyScale(4)

            contentItem: Text {

                text: comp_vars[index]
                color: delega.highlighted ? BSStyle.comboBoxItemTextHighlightedColor :
                                            BSStyle.comboBoxItemTextColor
                font.pixelSize: BSSizes.applyScale(16)
                font.family: "Roboto"
                font.weight: Font.Normal

                elide: Text.ElideNone
                verticalAlignment: Text.AlignVCenter
            }

            highlighted: comp_list.currentIndex === index && !_popup.not_valid_word

            background: Rectangle {
                color: delega.highlighted ? BSStyle.comboBoxItemHighlightedColor : "transparent"
                opacity: delega.highlighted ? 0.2 : 1
                radius: BSSizes.applyScale(14)
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                onPositionChanged:  {
                    if (containsMouse)
                        comp_list.currentIndex = index
                }
                onClicked: {
                    comp_list.currentIndex = index
                    compChoosed()
                }
            }
        }
    }

    background: Rectangle {
        color: "#FFFFFF"
        radius: BSSizes.applyScale(14)
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
