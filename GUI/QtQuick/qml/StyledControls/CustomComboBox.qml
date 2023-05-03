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

ComboBox {
    id: control

    property alias title_text: title.text
    property alias details_text: details.text
    property int fontSize: BSSizes.applyScale(16)
    property color fontColor: "#FFFFFF"

    property alias input_accept_input: input.acceptableInput
    property alias input_text: input.text
    property alias input_item: input

    signal textEdited()
    signal editingFinished()

    activeFocusOnTab: true
    focusPolicy: Qt.TabFocus

    leftPadding: BSSizes.applyScale(16)
    rightPadding: BSSizes.applyScale(36)
    topPadding: BSSizes.applyScale(16)
    bottomPadding: BSSizes.applyScale(16)

    contentItem: Rectangle {

        id: input_rect


        color: "transparent"

        Label {
            id: title

            anchors.top: parent.top
            anchors.topMargin: 0
            anchors.left: parent.left
            anchors.leftMargin: 0

            font.pixelSize: BSSizes.applyScale(13)
            font.family: "Roboto"
            font.weight: Font.Normal

            color: "#7A88B0"
        }

        Label {
            id: details

            anchors.bottom: parent.bottom
            anchors.bottomMargin: BSSizes.applyScale(1)
            anchors.right: parent.right
            anchors.rightMargin: 0

            font.pixelSize: BSSizes.applyScale(13)
            font.family: "Roboto"
            font.weight: Font.Normal

            color: "#7A88B0"
        }

        TextInput {
            id: input

            focus: true

            anchors.bottom: parent.bottom
            anchors.bottomMargin: 0
            anchors.left: parent.left
            anchors.leftMargin: 0

            width: details.text.length ? parent.width - details.width - BSSizes.applyScale(16) : parent.width
            height: BSSizes.applyScale(19)

            font.pixelSize: control.fontSize
            font.family: "Roboto"
            font.weight: Font.Normal

            color: control.fontColor

            text: control.currentText
            validator: control.validator
            enabled: control.editable

            clip: true

            onTextEdited : {
                control.textEdited()
            }
            
            onEditingFinished : {
                control.editingFinished()
            }

            Connections {
                target: control
                onCurrentTextChanged: input.text = control.currentText
            }
        }
    }

    indicator: Canvas {

        id: canvas

        x: control.width - width - BSSizes.applyScale(17)
        y: control.topPadding + (control.availableHeight - height) / 2
        width: BSSizes.applyScale(9)
        height: BSSizes.applyScale(6)

        contextType: "2d"

        Connections {

            target: control
            function onPressedChanged() {
                canvas.requestPaint()
            }
        }

        onPaint: {

            context.reset()
            context.moveTo(0, 0)
            context.lineTo(width, 0)
            context.lineTo(width / 2, height)
            context.closePath()
            context.fillStyle = control.popup.visible ? BSStyle.comboBoxIndicatorColor
                                                      : BSStyle.comboBoxPopupedIndicatorColor
            context.fill()
        }
    }

    background: Rectangle {

        color: "#020817"
        opacity: 1
        radius: BSSizes.applyScale(14)

        border.color: control.popup.visible ? BSStyle.comboBoxPopupedBorderColor :
                      (control.hovered ? BSStyle.comboBoxHoveredBorderColor :
                      (control.activeFocus ? BSStyle.comboBoxFocusedBorderColor : BSStyle.comboBoxBorderColor))
        border.width: 1

        implicitWidth: control.width
        implicitHeight: control.height
    }

    delegate: ItemDelegate {

        id: menuItem

        width: control.width - BSSizes.applyScale(12)
        height: BSSizes.applyScale(27)

        leftPadding: BSSizes.applyScale(6)
        topPadding: BSSizes.applyScale(4)
        bottomPadding: BSSizes.applyScale(4)

        contentItem: Text {

            text: control.textRole
                ? (Array.isArray(control.model) ? modelData[control.textRole] : model[control.textRole])
                : modelData
            color: menuItem.highlighted ? BSStyle.comboBoxItemTextHighlightedColor : ( menuItem.currented ? BSStyle.comboBoxItemTextCurrentColor : BSStyle.comboBoxItemTextColor)
            font.pixelSize: control.fontSize
            font.family: "Roboto"
            font.weight: Font.Normal

            elide: Text.ElideNone
            verticalAlignment: Text.AlignVCenter
        }

        highlighted: control.highlightedIndex === index
        property bool currented: control.currentIndex === index

        background: Rectangle {
            color: menuItem.highlighted ? BSStyle.comboBoxItemHighlightedColor : "transparent"
            opacity: menuItem.highlighted ? 0.2 : 1
            radius: BSSizes.applyScale(14)
        }
    }

    popup: Popup {
        id: _popup

        y: control.height - BSSizes.applyScale(1)
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
            color: "#FFFFFF"
            radius: BSSizes.applyScale(14)
        }
    }
}

