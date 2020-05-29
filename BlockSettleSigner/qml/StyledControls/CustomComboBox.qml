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
    spacing: 3
    rightPadding: 30 // workaround to decrease width of TextInput
    property alias maximumLength: input.maximumLength

    contentItem: TextInput {
        id: input
        text: control.displayText
        font: control.font
        color: { control.enabled ? BSStyle.comboBoxItemTextHighlightedColor : BSStyle.disabledTextColor }
        leftPadding: 7
        rightPadding: control.indicator.width + control.spacing
        verticalAlignment: Text.AlignVCenter
        clip: true
        readOnly: !editable
        validator: control.validator
    }

    indicator: Canvas {
        id: canvas
        x: control.width - width
        y: control.topPadding + (control.availableHeight - height) / 2
        width: 30
        height: 8
        contextType: "2d"

        Connections {
            target: control
            onPressedChanged: canvas.requestPaint()
        }

        onPaint: {
            context.reset();
            context.moveTo(0, 0);
            context.lineTo(8,8);
            context.lineTo(16, 0);
            context.lineTo(15, 0);
            context.lineTo(8,7);
            context.lineTo(1, 0);
            context.closePath();
            context.fillStyle = BSStyle.comboBoxItemTextHighlightedColor;
            context.fill();
        }
    }

    background: Rectangle {
        implicitWidth: 120
        color: { control.enabled ?  BSStyle.comboBoxBgColor :  BSStyle.disabledBgColor }
        implicitHeight: 25
        border.color: { control.enabled ? BSStyle.inputsBorderColor : BSStyle.disabledColor }
        border.width: control.visualFocus ? 2 : 1
        radius: 2
    }

    delegate: ItemDelegate {
        width: control.width
        id: menuItem

        contentItem: Text {
            text: modelData
            color: menuItem.highlighted ? BSStyle.comboBoxItemTextColor : BSStyle.comboBoxItemTextHighlightedColor
            font: control.font
            elide: Text.ElideNone
            verticalAlignment: Text.AlignVCenter
        }
        highlighted: control.highlightedIndex === index

        background: Rectangle {
            color: menuItem.highlighted ? BSStyle.comboBoxItemBgHighlightedColor : BSStyle.comboBoxItemBgColor
        }
    }

    popup: Popup {
        y: control.height - 1
        width: control.width
        implicitHeight: contentItem.implicitHeight
        padding: 1

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex

            ScrollIndicator.vertical: ScrollIndicator { }
        }

        background: Rectangle {
            color: BSStyle.comboBoxItemBgColor
            border.color: BSStyle.inputsBorderColor
            radius: 0
        }
    }
}

