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

    property string controlHint: "123"
    property int popupWidth: 400

    textRole: "coin"
    valueRole: "network"

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
                sourceSize.width: 80
                sourceSize.height: 80
                source: "image://coin/" + control.currentText + "-" + control.currentValue
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
        height: 80

        leftPadding: 6
        topPadding: 4
        bottomPadding: 4

        contentItem: Rectangle {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            color: "transparent"

            Row {
                spacing: 10
                anchors.fill: parent
                anchors.leftMargin: 10

                Image {
                    width: 40
                    height: 40
                    sourceSize.width: 40
                    sourceSize.height: 40
                    source: "image://coin/" + model["coin"] + "-" + model["network"]
                    anchors.verticalCenter: parent.verticalCenter
                }

                Column {
                    spacing: 10
                    width: parent.width - 50
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        text: model["name"]
                        color: "white"
                        font.pixelSize: 14
                        font.family: "Roboto"
                        font.weight: Font.SemiBold
                    }

                    Row {
                        spacing: 10

                        Text {
                            text: model["coin"]
                            color: "white"
                            font.pixelSize: 14
                            font.family: "Roboto"
                            font.weight: Font.Normal
                        }

                        Text {
                            text: model["network"]
                            color: "white"
                            font.pixelSize: 14
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
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            color: menuItem.highlighted ? "white" : "transparent"
            opacity: menuItem.highlighted ? 0.2 : 1
            radius: 4
        }
    }

    popup: Popup {
        id: _popup

        y: 0
        width: control.popupWidth
        height: 400
        padding: 6

        contentItem: Rectangle {
            color: "transparent"
            anchors.fill: parent
            
            Column {
                anchors.fill: parent

                Rectangle {
                    width: parent.width
                    height: 80
                    color: "transparent"

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 10
                        color: "transparent"
                        border.color: "white"
                        radius: 10
                    }
                    
                    TextInput {
                        anchors.fill: parent
                        color: "white"
                        leftPadding: 20
                        verticalAlignment: Text.AlignVCenter

                        onTextEdited: control.model.filter = text
                    }
                }

                ListView {
                    id: popup_item
                    width: parent.width
                    height: parent.height - 80

                    clip: true
                    model: control.popup.visible ? control.delegateModel : null
                    currentIndex: control.highlightedIndex

                    ScrollIndicator.vertical: ScrollIndicator { }
                }
            }
        }   

        background: Rectangle {
            color: "black"
            radius: 14

            border.width: 1
            border.color: "white"
        }
    }
}

