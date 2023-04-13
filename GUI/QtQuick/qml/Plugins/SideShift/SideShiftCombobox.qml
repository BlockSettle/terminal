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

import "../../BsStyles"

ComboBox {
    id: control

    width: BSSizes.applyScale(400)
    height: BSSizes.applyScale(50)

    activeFocusOnTab: true

    contentItem: Rectangle {
        id: input_rect
        color: "transparent"

        Text {
            anchors.fill: parent
            text: control.currentText
            font.pixelSize: BSSizes.applyScale(18)
            font.family: "Roboto"
            font.weight: Font.Bold
            color: "white"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            clip: true
        }
    }

    indicator: Item {}

    background: Rectangle {

        color: "#181414"
        opacity: 1
        radius: BSSizes.applyScale(4)

        border.color: control.popup.visible ? "white" :
                      (control.hovered ? "white" :
                      (control.activeFocus ? "white" : "gray"))
        border.width: 1

        implicitWidth: control.width
        implicitHeight: control.height
    }

    delegate: ItemDelegate {

        id: menuItem

        width: control.width - BSSizes.applyScale(12)
        height: BSSizes.applyScale(50)

        leftPadding: BSSizes.applyScale(6)
        topPadding: BSSizes.applyScale(4)
        bottomPadding: BSSizes.applyScale(4)

        contentItem: Text {

            text: control.textRole
                ? (Array.isArray(control.model) ? modelData[control.textRole] : model[control.textRole])
                : modelData
            color: "white"
            font.pixelSize: BSSizes.applyScale(14)
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
        padding: BSSizes.applyScale(6)

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
            color: "black"
            radius: BSSizes.applyScale(4)

            border.width: 1
            border.color: "white"
        }
    }
}

