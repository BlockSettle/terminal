/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/

import QtQuick 2.9
import QtQuick.Controls 2.3

import "../Styles"
import "../../../BsStyles"

ComboBox {
    id: control

    width: 400
    height: 50

    activeFocusOnTab: true

    property string comboboxHint

    contentItem: Rectangle {

        id: input_rect
        color: "transparent"

         Column {
            spacing: 4
            anchors.verticalCenter: parent.verticalCenter

            Text {
                text: control.comboboxHint
                font.pixelSize: 12
                font.family: "Roboto"
                font.weight: Font.Bold
                color: SideSwapStyles.secondaryTextColor
                leftPadding: 10
            }

            Text {
                text: control.currentText
                font.pixelSize: 18
                font.family: "Roboto"
                color: "white"
                leftPadding: 10
            }
         }
    }

    // indicator: Item {}

    background: Rectangle {

        color: SideSwapStyles.buttonSecondaryBackground
        opacity: 1
        radius: 4

        border.color: control.popup.visible ? SideSwapStyles.buttonBackground :
                      (control.hovered ? SideSwapStyles.buttonBackground :
                      (control.activeFocus ? SideSwapStyles.buttonBackground : SideSwapStyles.spacerColor))
        border.width: 1

        implicitWidth: control.width
        implicitHeight: control.height
    }

    delegate: ItemDelegate {

        id: menuItem

        width: control.width - 12
        height: 50

        leftPadding: 6
        topPadding: 4
        bottomPadding: 4

        contentItem: Text {

            text: control.textRole
                ? (Array.isArray(control.model) ? modelData[control.textRole] : model[control.textRole])
                : modelData
            color: "white"
            font.pixelSize: 14
            font.family: "Roboto"
            font.weight: Font.Normal

            elide: Text.ElideNone
            verticalAlignment: Text.AlignVCenter
        }

        highlighted: control.highlightedIndex === index
        property bool currented: control.currentIndex === index

        background: Rectangle {
            color: menuItem.highlighted ? "white" : "transparent"
            opacity: menuItem.highlighted ? 0.2 : 1
            radius: 4
        }
    }

    popup: Popup {
        id: _popup

        y: control.height - 1
        width: control.width
        padding: 6

        contentItem: ListView {
            id: popup_item

            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            //model: control.delegateModel
            currentIndex: control.highlightedIndex

            ScrollIndicator.vertical: ScrollIndicator { }
        }

        background: Rectangle {
            color: SideSwapStyles.buttonSecondaryBackground
            radius: 4

            border.width: 1
            border.color: SideSwapStyles.spacerColor
        }
    }
}

