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

import com.blocksettle.HwDeviceManager 1.0

import "../BsControls"
import "../StyledControls"
import "../js/helper.js" as JsHelper
import "../BsStyles"

Item {
    id: root

    property var hwWalletInfo
    property bool readyForImport: hwList.deviceIndex !== -1
    property bool isNoDevice: hwDeviceManager.devices.rowCount() === 0
    property bool isScanning: hwDeviceManager.isScanning
    property bool isImporting: false

    signal pubKeyReady();
    signal failed();

    Connections {
        target: hwDeviceManager
        onPublicKeyReady: {
            hwWalletInfo = walletInfo;
            isImporting = false;
            root.pubKeyReady()
        }
        onRequestPinMatrix: JsHelper.showPinMatrix(hwList.deviceIndex);
        onOperationFailed: {
            hwWalletInfo = {};
            isImporting = false;
            root.failed();
        }
    }

    function init() {
        rescan();
    }

    function release() {
        hwDeviceManager.releaseDevices();
    }

    function importXpub() {
        hwDeviceManager.requestPublicKey(hwList.deviceIndex);
        isImporting = true;
    }

    function rescan() {
        hwDeviceManager.scanDevices();
    }

    Item {
        id: hwNoDevice

        anchors.fill: parent
        visible: root.isNoDevice

        Text {
            id: noDeviceText
            text: qsTr('No hardware device detected.\n'
                       + 'To trigger a rescan, press "Rescan" button.\n\n'
                       + 'If you device is not detected:\n'
                       + ' • On Windows, go to "Settings" -> "Devices" -> "Connected devices" and click "Remote device".Then plug in your device again.\n'
                       + ' • On linux/OSX you might have to add a new permission to your udev rules.'
                       );

            anchors {
                top: parent.top
                left: parent.left
                right: parent.right
            }

            leftPadding: 5
            color: BSStyle.labelsTextColor
            font.pixelSize: 11
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignTop
            elide: Text.ElideRight
            wrapMode: Text.Wrap
        }

    }

    ListView {
        id: hwList
        visible: !isNoDevice
        anchors.fill: parent

        model: hwDeviceManager.devices

        property int deviceIndex: -1

        highlight: Rectangle {
            color: BSStyle.comboBoxItemBgHighlightedColor
            anchors.leftMargin: 5
            anchors.rightMargin: 5
        }

        delegate: Rectangle {
            id: delegateRoot

            width: parent.width
            height: 20

            color: index % 2 === 0 ? "transparent" : "#8027363b"
            property color textColor: hwList.currentIndex === index ? "white" :
                                        enabled ? BSStyle.labelsTextColor : BSStyle.disabledColor

            RowLayout {
                anchors.fill: parent
                Text {
                    Layout.fillWidth: true
                    height: parent.height

                    leftPadding: 10
                    text: model.label + "(" + model.vendor + ")"
                    enabled: model.pairedWallet.length === 0
                    color: textColor
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    height: parent.height

                    rightPadding: 10
                    text: model.pairedWallet.length ? "Imported(" + model.pairedWallet + ")" : "New Device"
                    enabled: model.pairedWallet.length === 0
                    color: textColor
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignRight
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }

            MouseArea {
                anchors.fill: parent
                enabled: model.pairedWallet.length === 0
                onClicked: {
                    if (hwList.currentIndex === index) {
                        return;
                    }

                    hwList.currentIndex = index
                }
            }

            Connections {
                target: hwList
                onCurrentIndexChanged: {
                    if (hwList.currentIndex === index) {
                        hwList.deviceIndex = model.pairedWallet.length === 0 ? index : -1
                        root.readyForImport = (hwList.deviceIndex !== -1);
                        console.log("Current index changed2", hwList.currentIndex)
                    }
                }
            }
        }

        onCountChanged: {
            if (count !== 0 && hwList.currentIndex === -1)
                hwList.currentIndex = 0;
        }

    }
}
