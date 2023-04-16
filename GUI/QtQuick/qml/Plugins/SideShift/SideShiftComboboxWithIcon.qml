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

    width: BSSizes.applyScale(200)
    height: BSSizes.applyScale(200)

    property string controlHint: "123"
    property int popupWidth: BSSizes.applyScale(400)

    textRole: "coin"
    valueRole: "network"

    activeFocusOnTab: true

    contentItem: Rectangle {

        id: input_rect
        color: "transparent"

         Column {
            spacing: BSSizes.applyScale(8)
            anchors.centerIn: parent

            Image {
                width: BSSizes.applyScale(80)
                height: BSSizes.applyScale(80)
                sourceSize.width: BSSizes.applyScale(80)
                sourceSize.height: BSSizes.applyScale(80)
                source: "image://coin/" + control.currentText + "-" + control.currentValue
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Text {
                text: control.controlHint
                font.pixelSize: BSSizes.applyScale(14)
                font.family: "Roboto"
                font.weight: Font.Normal
                color: "white"
                topPadding: BSSizes.applyScale(15)
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Text {
                text: control.currentText
                font.pixelSize: BSSizes.applyScale(18)
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
        radius: BSSizes.applyScale(14)

        border.color: control.popup.visible ? "white" :
                      (control.hovered ? "white" :
                      (control.activeFocus ? "white" : "gray"))
        border.width: 1

        implicitWidth: control.width
        implicitHeight: control.height
    }

    delegate: ItemDelegate {

        id: menuItem

        width: control.popupWidth - BSSizes.applyScale(12)
        height: BSSizes.applyScale(80)

        leftPadding: BSSizes.applyScale(6)
        topPadding: BSSizes.applyScale(4)
        bottomPadding: BSSizes.applyScale(4)

        contentItem: Rectangle {
            anchors.fill: parent
            anchors.leftMargin: BSSizes.applyScale(10)
            anchors.rightMargin: BSSizes.applyScale(10)
            color: "transparent"

            Row {
                spacing: BSSizes.applyScale(10)
                anchors.fill: parent
                anchors.leftMargin: BSSizes.applyScale(10)

                Image {
                    width: BSSizes.applyScale(40)
                    height: BSSizes.applyScale(40)
                    sourceSize.width: BSSizes.applyScale(40)
                    sourceSize.height: BSSizes.applyScale(40)
                    source: "image://coin/" + model["coin"] + "-" + model["network"]
                    anchors.verticalCenter: parent.verticalCenter
                }

                Column {
                    spacing: BSSizes.applyScale(10)
                    width: parent.width - BSSizes.applyScale(50)
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        text: model["name"]
                        color: "white"
                        font.pixelSize: BSSizes.applyScale(14)
                        font.family: "Roboto"
                        font.weight: Font.SemiBold
                    }

                    Row {
                        spacing: BSSizes.applyScale(10)

                        Text {
                            text: model["coin"]
                            color: "white"
                            font.pixelSize: BSSizes.applyScale(14)
                            font.family: "Roboto"
                            font.weight: Font.Normal
                        }

                        Text {
                            text: model["network"]
                            color: "white"
                            font.pixelSize: BSSizes.applyScale(14)
                            font.family: "Roboto"
                            font.weight: Font.Normal
                        }
                    }
                }
            }
        }

        highlighted: control.highlightedIndex === index
        property bool currented: control.currentIndex === index

        background: Rectangle {
            anchors.fill: parent
            anchors.leftMargin: BSSizes.applyScale(10)
            anchors.rightMargin: BSSizes.applyScale(10)
            color: menuItem.highlighted ? "white" : "transparent"
            opacity: menuItem.highlighted ? 0.2 : 1
            radius: BSSizes.applyScale(4)
        }
    }

    popup: Popup {
        id: _popup

        y: 0
        width: control.popupWidth
        height: BSSizes.applyScale(400)
        padding: BSSizes.applyScale(6)

        contentItem: Rectangle {
            color: "transparent"
            anchors.fill: parent
            
            Column {
                anchors.fill: parent

                Rectangle {
                    width: parent.width
                    height: BSSizes.applyScale(80)
                    color: "transparent"

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: BSSizes.applyScale(10)
                        color: "transparent"
                        border.color: "white"
                        radius: BSSizes.applyScale(10)
                    }
                    
                    TextInput {
                        id: search_input
                        anchors.fill: parent
                        color: "white"
                        leftPadding: BSSizes.applyScale(20)
                        verticalAlignment: Text.AlignVCenter

                        onTextEdited: control.model.filter = text
                    }
                }

                ListView {
                    id: popup_item
                    width: parent.width
                    height: parent.height - BSSizes.applyScale(80)

                    clip: true
                    model: control.popup.visible ? control.delegateModel : null
                    currentIndex: control.highlightedIndex

                    ScrollIndicator.vertical: ScrollIndicator { }

                    maximumFlickVelocity: 1000
                }
            }
        }   

        background: Rectangle {
            color: "black"
            radius: BSSizes.applyScale(14)

            border.width: 1
            border.color: "white"
        }

        onOpened: search_input.forceActiveFocus()
    }
}
