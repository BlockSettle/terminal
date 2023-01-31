/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.15
import QtQuick.Controls 2.3

ComboBox {
    id: control

    width: 263
    height: 53

    property color font_color: "#ffffff"

    property color hightligh_color: "gray"
    property color background_color: "#020817"
    property color background_border_color: "#3C435A"
    property int background_border_width: 1
    property int background_radius: 14

    property int text_margin: 16
    property int text_size: 14

    property int indicator_width: 10
    property int indicator_height: 6
    property color indicator_fill_color: "#DCE2FF"

    model: ['Alex wallet', 'B', 'C', 'D']

    activeFocusOnTab: true

    font.pixelSize: 14
    font.family: "Roboto"
    font.weight: Font.Normal

    contentItem: Rectangle {

        width: control.width
        height: control.height
        color: "transparent"
        
        Text {
            text: control.currentText
            color: control.font_color
            font: control.font
            verticalAlignment: Text.AlignVCenter

            anchors.fill: parent
            anchors.margins: control.text_margin
        }
    }

    background: Rectangle {
        color: control.background_color
        radius: control.background_radius

        border.color: control.background_border_color
        border.width: control.background_border_width
    }

    indicator: Canvas {
        id: indicator_shape

        width: control.indicator_width
        height: control.indicator_height

        anchors.margins: control.text_margin
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter


        transform: Rotation {
            origin.x: control.popup.visible ? indicator_shape.width / 2 : 0
            origin.y: control.popup.visible ? indicator_shape.height / 2 : 0
            angle: control.popup.visible ? 180 : 0
        }

        onPaint: {
            var ctx = getContext("2d")
            ctx.lineWidth = 1
            ctx.strokeStyle = "transparent"
            ctx.fillStyle = control.indicator_fill_color

            ctx.beginPath()
            ctx.moveTo(0, 0)
            ctx.lineTo(indicator_shape.width,0)
            ctx.lineTo(indicator_shape.width / 2, indicator_shape.height)
            ctx.lineTo(0, 0)
            ctx.closePath()
            ctx.fill()
            ctx.stroke()
        }
    }

    delegate: ItemDelegate {
        id: delegate_item
        
        width: control.width
        height: 27

        contentItem: Rectangle {
            radius: control.background_radius
            color: delegate_item.highlighted ? "#3345A6FF": "transparent"

            anchors.fill: parent
            anchors.margins: 4

            Text {
                text: model[textRole]
                color: delegate_item.highlighted ? "#45A6FF" : "black"

                font: control.font
                verticalAlignment: Text.AlignVCenter

                anchors.fill: parent
                anchors.margins: control.text_margin - parent.anchors.margins
            }
        }

        background: Rectangle {
            color: 'transparent'
        }

        highlighted: control.highlightedIndex === index
    }

    popup: Popup {
        y: control.height
        width: control.width
        padding: 0

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex

            ScrollIndicator.vertical: ScrollIndicator { }
        }

        background: Rectangle {
            color: "#FFFFFFFF"
            radius: control.background_radius
        }
    }
}

