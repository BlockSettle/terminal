/*

***********************************************************************************
* Copyright (C) 2020 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.9
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.2
import Qt.labs.platform 1.1

import com.blocksettle.HwDeviceManager 1.0

import "../BsControls"
import "../StyledControls"
import "../js/helper.js" as JsHelper
import "../BsStyles"

CustomDialog {
    width: 380
    height: 100
    property int deviceIndex: -1
    property bool allowedOnDevice: false

    cHeaderItem: CustomHeader {
        Layout.leftMargin: 10
        text: "Please enter passphrase"
    }

    cContentItem: CustomPasswordTextInput {
        id: pinInputField
        Layout.fillWidth: true
        Layout.preferredHeight: 30
        Layout.leftMargin: 5
        Layout.rightMargin: 5

        text: ""
    }

    cFooterItem: RowLayout {
        CustomButtonBar {
            Layout.fillWidth: true

            CustomButton {
                id: btnCancel
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                text: qsTr("Cancel")
                onClicked: {
                    hwDeviceManager.cancel(deviceIndex)
                    close();
                }
            }

            CustomButton {
                anchors.left: btnCancel.right
                anchors.right: btnAccept.left
                anchors.bottom: parent.bottom
                text: qsTr("On Device")
                visible: allowedOnDevice
                onClicked: {
                    hwDeviceManager.setPassphrase(deviceIndex, "", true)
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
                    hwDeviceManager.setPassphrase(deviceIndex, pinInputField.text, false)
                    close();
                }
            }
        }
    }
}
