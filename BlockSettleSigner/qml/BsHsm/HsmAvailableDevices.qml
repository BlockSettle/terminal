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

Item {
    id: root

    property var hsmWalletInfo
    property bool readyForImport: hsmList.deviceIndex !== -1
    property bool isNoDevice: hsmDeviceManager.devices.rowCount() === 0
    property bool isScanning: hsmDeviceManager.isScanning
    property bool isImporting: false

    signal pubKeyReady();
    signal failed();

    Connections {
        target: hsmDeviceManager
        onPublicKeyReady: {
            hsmWalletInfo = walletInfo;
            isImporting = false;
            root.pubKeyReady()
        }
        onRequestPinMatrix: JsHelper.showPinMatrix(hsmList.deviceIndex);
        onOperationFailed: {
            hsmWalletInfo = {};
            isImporting = false;
            root.failed();
        }
    }

    function init() {
        rescan();
    }

    function release() {
        hsmDeviceManager.releaseDevices();
    }

    function importXpub() {
        hsmDeviceManager.requestPublicKey(hsmList.deviceIndex);
        isImporting = true;
    }

    function rescan() {
        hsmDeviceManager.scanDevices();
    }

    Item {
        id: hsmNoDevice

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
        id: hsmList
        visible: !isNoDevice
        anchors.fill: parent

        model: hsmDeviceManager.devices

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
            property color textColor: hsmList.currentIndex === index ? "white" :
                                        enabled ? BSStyle.labelsTextColor : BSStyle.disabledColor

            RowLayout {
                anchors.fill: parent
                Text {
                    width: parent.width * 3 / 4
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
                    width: parent.width  * 1 / 4
                    height: parent.height

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
                    if (hsmList.currentIndex === index) {
                        return;
                    }

                    hsmList.currentIndex = index
                }
            }

            Connections {
                target: hsmList
                onCurrentIndexChanged: {
                    if (hsmList.currentIndex === index) {
                        hsmList.deviceIndex = model.pairedWallet.length === 0 ? index : -1
                        root.readyForImport = (hsmList.deviceIndex !== -1);
                        console.log("Current index changed2", hsmList.currentIndex)
                    }
                }
            }
        }

        onCountChanged: {
            if (count !== 0 && hsmList.currentIndex === -1)
                hsmList.currentIndex = 0;
        }

    }
}
