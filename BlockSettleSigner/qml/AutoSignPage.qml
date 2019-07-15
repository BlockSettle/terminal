import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

import com.blocksettle.WalletsProxy 1.0
import com.blocksettle.WalletInfo 1.0
import com.blocksettle.QmlFactory 1.0
import com.blocksettle.QPasswordData 1.0
import com.blocksettle.AutheIDClient 1.0
import com.blocksettle.AuthSignWalletObject 1.0

import "StyledControls"
import "BsControls"
import "js/helper.js" as JsHelper

Item {
    id: root
    property int currentIndex: 0

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

            GridLayout {
                columns: 2
                Layout.fillWidth: true
                Layout.topMargin: 5
                Layout.leftMargin: 10
                Layout.rightMargin: 10

                CustomHeader {
                    Layout.columnSpan: 2
                    text: qsTr("Controls")
                    enabled: !signerStatus.offline
                    height: 25
                    Layout.fillWidth: true
                    Layout.preferredHeight: 25
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 25
                    enabled: !signerStatus.offline
                    CustomLabel {
                        Layout.fillWidth: true
                        text: qsTr("Wallet")
                    }
                }

                Loader {
                    active: walletsProxy.loaded
                    sourceComponent:
                        CustomComboBox {
                        width: 150
                        height: 25
                        enabled: !signerStatus.autoSignActive && !signerStatus.offline
                        model: walletsProxy.walletNames
                        currentIndex: walletsProxy.indexOfWalletId(signerSettings.autoSignWallet)
                        onActivated: {
                            root.currentIndex = currentIndex
                            signerSettings.autoSignWallet = walletsProxy.walletIdForIndex(currentIndex)
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 25

                    CustomLabel {
                        Layout.fillWidth: true
                        enabled: !signerStatus.offline
                        text: qsTr("Auto-Sign")
                    }
                }

                CustomSwitch {
                    id: autoSignSwitch
                    Layout.alignment: Qt.AlignRight
                    enabled: !signerStatus.offline
                    checked: signerStatus.autoSignActive
                    onClicked: {
                        var walletInfo = qmlFactory.createWalletInfo(signerSettings.autoSignWallet)
                        var newState = checked
                        // don't change switch state by click
                        // change state by received signal
                        checked = !newState

                        if (newState) {
                            var autoSignCallback = function(success, errorMsg) {
                                if (success) {
                                    JsHelper.messageBox(BSMessageBox.Type.Success
                                        , qsTr("Wallet Auto Sign")
                                        , qsTr("Auto Signing enabled for wallet %1")
                                            .arg(walletInfo.rootId))
                                }
                                else {
                                    JsHelper.messageBox(BSMessageBox.Type.Critical
                                        , qsTr("Wallet Auto Sign")
                                        , qsTr("Failed to enable auto signing.")
                                        , errorString)
                                }
                            }

                            if (walletInfo.encType === QPasswordData.Password) {
                                var passwordDialog = Qt.createComponent("BsControls/BSPasswordInput.qml").createObject(mainWindow);
                                passwordDialog.type = BSPasswordInput.Type.Request
                                passwordDialog.open()
                                passwordDialog.bsAccepted.connect(function() {
                                    var passwordData = qmlFactory.createPasswordData()
                                    passwordData.encType = QPasswordData.Password
                                    passwordData.encKey = ""
                                    passwordData.textPassword = passwordDialog.enteredPassword

                                    signerStatus.activateAutoSign(walletInfo.rootId, passwordData, true, autoSignCallback)
                                })
                            }
                            else if (walletInfo.encType === QPasswordData.Auth) {
                                JsHelper.requesteIdAuth(AutheIDClient.SignWallet, walletInfo, function(passwordData){
                                    signerStatus.activateAutoSign(walletInfo.rootId, passwordData, true, autoSignCallback)
                                })
                            }

                        }
                        else {
                            var autoSignDisableCallback = function(success, errorMsg) {
                                if (success) {
                                    JsHelper.messageBox(BSMessageBox.Type.Success
                                        , qsTr("Wallet Auto Sign")
                                        , qsTr("Auto Signing disabled for wallet %1")
                                            .arg(walletInfo.rootId))
                                }
                                else {
                                    JsHelper.messageBox(BSMessageBox.Type.Critical
                                        , qsTr("Wallet Auto Sign")
                                        , qsTr("Failed to disable auto signing.")
                                        , errorString)
                                }
                            }

                            signerStatus.activateAutoSign(walletInfo.rootId, 0, false, autoSignDisableCallback)
                        }
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
                    model: [ "Unlimited", "0.1", "0.5", "1", "2", "5"]
                    maximumLength: 9

                    // FIXME: uncomment when limits will be fixed
                    // displayText: signerSettings.autoSignUnlimited ? qsTr("Unlimited") : signerSettings.limitAutoSignXbt
                    onCurrentTextChanged: {
                        if (currentText !== qsTr("Unlimited")) {
                            signerSettings.limitAutoSignXbt = currentText
                        }
                        else {
                            signerSettings.limitAutoSignXbt = 0
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
                    model: [ "Unlimited", "30m", "1h", "6h", "12h", "24h"]
                    maximumLength: 9

                    // FIXME: uncomment when limits will be fixed
                    // displayText: signerSettings.limitAutoSignTime ? signerSettings.limitAutoSignTime : qsTr("Unlimited")
                    onCurrentTextChanged: {
                        if (currentText !== qsTr("Unlimited")) {
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
