/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.9
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.2
import Qt.labs.platform 1.1

import com.blocksettle.HSMDeviceManager 1.0

import "../BsControls"
import "../StyledControls"
import "../js/helper.js" as JsHelper
import "../BsStyles"

CustomDialog {
    width: 300
    height: 300
    property int deviceIndex: -1

    cContentItem: ColumnLayout {
        spacing: 5

        GridLayout {

            rows: 3
            columns: 3

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 5
            Layout.rightMargin: 5

            Repeater {
                model: [7, 8, 9, 4, 5, 6, 1, 2, 3]
                delegate: Rectangle {
                    color: "transparent"
                    border.width: 1
                    border.color: BSStyle.inputsBorderColor

                    Layout.fillHeight: true
                    Layout.fillWidth: true


                    Text {
                        anchors.fill: parent
                        text: "?"
                        color: BSStyle.inputsBorderColor
                        font.pixelSize: 15
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            pinInputField.text += modelData;
                        }
                    }
                }
            }
        }

        CustomPasswordTextInput {
            id: pinInputField
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            Layout.leftMargin: 5
            Layout.rightMargin: 5

            text: ""
        }
    }

    cFooterItem: RowLayout {
        CustomButtonBar {
            Layout.fillWidth: true

            CustomButton {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                text: qsTr("Cancel")
                onClicked: {
                    hsmDeviceManager.cancel(deviceIndex)
                    close();
                }
            }

            CustomButton {
                id: btnAccept
                primary: true
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                text: qsTr("Accept")
                onClicked: {
                    hsmDeviceManager.setMatrixPin(deviceIndex, pinInputField.text)
                    close();
                }
            }

        }
    }
}
