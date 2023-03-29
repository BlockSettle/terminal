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

    width: 200
    height: 200

    property string controlHint
    property int popupWidth
    property var networkControl: null

    activeFocusOnTab: true

    contentItem: Rectangle {

        id: input_rect
        color: "transparent"

         Column {
            spacing: 8
            anchors.centerIn: parent

            Image {
                width: 80
                height: 80
                source: (networkControl === null) ? "qrc:/images/sideshift_btc.png"
                          : "image://coin/" + control.currentText + "-" + networkControl.currentText
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Text {
                text: control.controlHint
                font.pixelSize: 14
                font.family: "Roboto"
                font.weight: Font.Normal
                color: "white"
                topPadding: 15
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Text {
                text: control.currentText
                font.pixelSize: 18
                font.family: "Roboto"
                font.weight: Font.Bold
                color: "white"
                horizontalAlignment: Text.AlignHCenter
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
    }

    indicator: Item {}

    background: Rectangle {

        color: "#181414"
        opacity: 1
        radius: 14

        border.color: control.popup.visible ? "white" :
                      (control.hovered ? "white" :
                      (control.activeFocus ? "white" : "gray"))
        border.width: 1

        implicitWidth: control.width
        implicitHeight: control.height
    }

    delegate: ItemDelegate {

        id: menuItem

        width: control.popupWidth - 12
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
        width: control.popupWidth
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
            color: "black"
            radius: 14

            border.width: 1
            border.color: "white"
        }
    }
}

