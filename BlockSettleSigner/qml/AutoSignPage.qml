/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

import com.blocksettle.WalletsProxy 1.0
import com.blocksettle.QmlFactory 1.0
import com.blocksettle.QPasswordData 1.0
import com.blocksettle.AutheIDClient 1.0
import com.blocksettle.AuthSignWalletObject 1.0

import "StyledControls"
import "BsControls"
import "js/helper.js" as JsHelper

Item {
    id: root
    property bool autoSignAllowed: false
    property string unlimText: qsTr("Unlimited")


    Connections {
        target: signerStatus
        onAutoSignActiveChanged: {
            autoSignSwitch.checked = signerStatus.autoSignActive
        }
    }

    ScrollView {
        anchors.fill: parent
        clip: true

        ColumnLayout {
            width: parent.parent.width

            CustomHeader {
                Layout.columnSpan: 2
                text: qsTr("Controls")
                enabled: !signerStatus.offline
                height: 25
                Layout.fillWidth: true
                Layout.preferredHeight: 25
                Layout.topMargin: 5
                Layout.leftMargin: 10
                Layout.rightMargin: 10
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                Layout.preferredHeight: 25
                enabled: !signerStatus.offline
                CustomLabel {
                    Layout.fillWidth: true
                    text: qsTr("Wallet")
                }

                CustomComboBox {
                    id: cbWallets
                    Layout.preferredWidth: 150
                    height: 25
                    enabled: !signerStatus.autoSignActive && !signerStatus.offline
                    model: walletsProxy.priWalletNames
                    onActivated: {
                        let walletId = walletsProxy.walletIdForName(currentText)
                        signerSettings.autoSignWallet = walletId
                        autoSignAllowed = !walletsProxy.isWatchingOnlyWallet(walletId)
                    }

                    Connections {
                        target: walletsProxy
                        onWalletsChanged: {
//                            cbWallets.currentIndex = walletsProxy.indexOfWalletId(signerSettings.autoSignWallet)
                            autoSignAllowed = !walletsProxy.isWatchingOnlyWallet(signerSettings.autoSignWallet)
                        }
                    }
                }
            }


            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 25
                Layout.leftMargin: 10
                Layout.rightMargin: 10

                CustomLabel {
                    Layout.fillWidth: true
                    enabled: !signerStatus.offline
                    text: qsTr("Auto-Sign")
                }

                CustomSwitch {
                    id: autoSignSwitch
                    Layout.alignment: Qt.AlignRight
                    enabled: !signerStatus.offline && autoSignAllowed
                    checked: signerStatus.autoSignActive
                    onClicked: {
                        var newState = checked
                        // don't change switch state by click
                        // change state by received signal
                        checked = !newState

                        if (signerSettings.autoSignWallet.length === 0) {
                            let walletId = walletsProxy.walletIdForName(cbWallets.currentText)
                            signerSettings.autoSignWallet = walletId
                        }

                        JsHelper.tryChangeAutoSign(newState, signerSettings.autoSignWallet, true)
                    }
                }
            }


            SettingsGrid {
                id: gridLimits
                columns: 3
                enabled: !signerStatus.offline

                CustomHeader {
                    Layout.fillWidth: true
                    Layout.columnSpan: 3
                    text: qsTr("Details")
                    Layout.preferredHeight: 25
                    enabled: !signerStatus.offline
                }

                CustomLabel {
                    text: qsTr("XBT spend limit")
                    enabled: !signerStatus.offline
                }
                CustomLabel {
                    Layout.fillWidth: true
                }

                CustomComboBox {
                    id: limitAutoSignXbt
                    Layout.preferredWidth: 150
                    height: 25
                    enabled: !signerStatus.autoSignActive && !signerStatus.offline
                    editable: true
                    model: [ unlimText, "0.1", "0.5", "1", "2", "5"]
                    maximumLength: 9

                    onCurrentIndexChanged: {
                        limitAutoSignXbt.displayText = undefined
                        if (currentIndex === 0) {
                            signerSettings.limitAutoSignXbt = 0
                        }
                        else if (currentIndex < 0) {
                            limitAutoSignXbt.displayText = signerSettings.limitAutoSignXbt
                        }
                        else {
                            let value = parseFloat(model[currentIndex])
                            signerSettings.limitAutoSignXbt = value
                        }
                    }
                    onAccepted: {
                        let value = parseFloat(editText)
                        if ((value > 0.0) && (value <= 1000)) {
                            signerSettings.limitAutoSignXbt = editText
                        }
                    }
                    validator: RegExpValidator {
                        regExp: /^[0-9]*\.?[0-9]*$/
                    }
                }

                CustomLabel {
                    text: qsTr("Time limit")
                    enabled: !signerStatus.offline
                }
                CustomLabel {
                    Layout.fillWidth: true
                }
                CustomComboBox {
                    id: limitAutoSignTime
                    Layout.preferredWidth: 150
                    height: 25
                    enabled: !signerStatus.autoSignActive && !signerStatus.offline
                    editable: true
                    model: [ unlimText, "30m", "1h", "6h", "12h", "24h"]
                    maximumLength: 9

                    onCurrentTextChanged: {
                        if (currentText !== unlimText) {
                            signerSettings.limitAutoSignTime = text
                        }
                        else {
                            signerSettings.limitAutoSignTime = 0
                        }
                    }
                    validator: RegExpValidator {
                        regExp: /^(?:\d+(h|hour|m|min|minute|s|sec|second)?\s*)*$/
                    }
                }
            }
        }
    }

    function storeSettings() {
        if (signerSettings.limitAutoSignXbt !== limitAutoSignXbt.displayText) {
            if (limitAutoSignXbt.displayText !== qsTr("Unlimited")) {
                signerSettings.limitAutoSignXbt = limitAutoSignXbt.displayText
            }
        }

        signerSettings.limitAutoSignTime = limitAutoSignTime.displayText
    }

}
