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

import com.blocksettle.WalletInfo 1.0
import com.blocksettle.HwDeviceManager 1.0

import "../BsControls"
import "../StyledControls"
import "../BsStyles"
import "../BsHw"
import "../js/helper.js" as JsHelper


CustomTitleDialogWindow {
    id: root

    property WalletInfo walletInfo: WalletInfo{}

    property bool acceptable: !hwDeviceList.isScanning && !hwDeviceList.isImporting && (hwDeviceList.readyForImport || hwDeviceList.isNoDevice)

    property int inputLabelsWidth: 110

    title: qsTr("Import Wallet")
    width: 600
    height: 250
    abortBoxType: BSAbortBox.AbortType.WalletImport

    onAboutToShow: hwDeviceList.init()
    onAboutToHide: hwDeviceList.release();

    onEnterPressed: {
        if (btnAccept.enabled) btnAccept.onClicked()
    }

    cContentItem: ColumnLayout {
        id: mainLayout
        spacing: 10

        CustomHeader {
            id: headerText
            text: qsTr("Hardware Device")
            Layout.fillWidth: true
            Layout.preferredHeight: 25
            Layout.topMargin: 5
            Layout.leftMargin: 10
            Layout.rightMargin: 10
        }

        StackLayout {
            currentIndex: hwDeviceList.isNoDevice ? 1 : 0
            Layout.fillWidth: true

            ColumnLayout {
                id: fullImportTab

                RowLayout {
                    Layout.topMargin: 0
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                    Layout.fillWidth: true

                    ColumnLayout {
                        id: selectLayout
                        Layout.fillWidth: true

                        // HARDWARE DEVICES
                        HwAvailableDevices {
                            id: hwDeviceList

                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            onPubKeyReady: {
                                importWoWallet();
                            }

                            onFailed: {
                                JsHelper.messageBox(BSMessageBox.Type.Critical
                                    , qsTr("Import Failed"), qsTr("Import WO-wallet failed:\n") + msg)
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                id: noDevicesAvailable

                RowLayout {
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10

                    CustomLabel {
                        Layout.fillWidth: true
                        text: qsTr("No hardware device was detected.\nPlease ensure your device is properly connected and press the \"Rescan\" button.")
                    }
                }
            }

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
                    JsHelper.openAbortBox(root, abortBoxType)
                }
            }

            CustomButton {
                id: btnAccept
                primary: true
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                text: hwDeviceList.isNoDevice ? qsTr("Rescan") : qsTr("Import")
                enabled: acceptable

                onClicked: {
                    if (hwDeviceList.readyForImport) {
                        hwDeviceList.importXpub();
                    } else if (hwDeviceList.isNoDevice) {
                        hwDeviceList.rescan();
                    }

                }
            }
        }
    }

    function applyDialogClosing() {
        JsHelper.openAbortBox(root, abortBoxType);
        return false;
    }

    function importWoWallet() {
        var importCallback = function(success, id, name, desc) {
            if (success) {
                let walletInfo = qmlFactory.createWalletInfo()
                walletInfo.walletId = id
                walletInfo.name = name
                walletInfo.desc = desc

                let type = BSResultBox.ResultType.HwWallet

                var mb = JsHelper.resultBox(type, true, walletInfo)
                mb.bsAccepted.connect(acceptAnimated)
            }
            else {
                JsHelper.messageBox(BSMessageBox.Type.Critical
                    , qsTr("Import Failed"), qsTr("Import WO-wallet failed:\n") + msg)
            }
        }

        walletsProxy.importHwWallet(hwDeviceList.hwWalletInfo, importCallback)
    }
}
