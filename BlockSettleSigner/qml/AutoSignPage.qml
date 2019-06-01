import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Dialogs 1.2
import QtQuick.Layouts 1.3

import com.blocksettle.WalletsProxy 1.0
import com.blocksettle.WalletInfo 1.0
import com.blocksettle.QmlFactory 1.0
import com.blocksettle.QPasswordData 1.0

import "StyledControls"
import "BsControls"
import "js/helper.js" as JsHelper

Item {
    id: root
    property int currentIndex: 0

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
                    height: 25
                    Layout.fillWidth: true
                    Layout.preferredHeight: 25
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 25
                    CustomLabel {
                        Layout.fillWidth: true
                        text: qsTr("Wallet")
                    }
                }

                Loader {
                    active: walletsProxy.loaded
                    sourceComponent: CustomComboBox {
                        width: 150
                        height: 25
                        enabled: !signerStatus.autoSignActive
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
                        text: qsTr("Auto-Sign")
                    }
                }

                CustomSwitch {
                    Layout.alignment: Qt.AlignRight
                    visible: !signerStatus.offline
                    checked: signerStatus.autoSignActive
                    onClicked: {
                        if (checked) {
                            var walletInfo = qmlFactory.createWalletInfo(signerSettings.autoSignWallet)

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
                                console.log("passwordDialog Password ")

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
                                console.log("passwordDialog Auth ")

                                JsHelper.requesteIdAuth(AutheIDClient.SignWallet, walletInfo, function(passwordData){
                                    signerStatus.activateAutoSign(walletInfo.rootId, passwordData, true, autoSignCallback)
                                })
                            }

                            //signerStatus.activateAutoSign()
                        }
                        else {
                            //signerStatus.deactivateAutoSign()
                        }
                    }
                }
            }

            SettingsGrid {
                id: gridLimits
                columns: 3

                CustomHeader {
                    Layout.fillWidth: true
                    Layout.columnSpan: 3
                    text: qsTr("Details")
                    Layout.preferredHeight: 25
                }

                CustomLabel {
                    text: qsTr("XBT spend limit")
                }
                CustomLabel {
                    Layout.fillWidth: true
                }
                CustomTextInput {
                    Layout.preferredWidth: 150
                    text: signerSettings.autoSignUnlimited ? qsTr("Unlimited") : signerSettings.limitAutoSignXbt
                    selectByMouse: true
                    id: limitAutoSignXbt
                    validator: RegExpValidator {
                        regExp: /^[0-9]*\.?[0-9]*$/
                    }
                    onEditingFinished: {
                        if (text !== qsTr("Unlimited")) {
                            signerSettings.limitAutoSignXbt = text
                        }
                    }
                }

                CustomLabel {
                    text: qsTr("Time limit")
                }
                CustomLabel {
                    Layout.fillWidth: true
                }
                CustomTextInput {
                    Layout.preferredWidth: 150
                    placeholderText: "e.g. 1h or 15m or 600s or combined"
                    selectByMouse: true
                    text: signerSettings.limitAutoSignTime ? signerSettings.limitAutoSignTime : qsTr("Unlimited")
                    id: limitAutoSignTime
                    validator: RegExpValidator {
                        regExp: /^(?:\d+(h|hour|m|min|minute|s|sec|second)?\s*)*$/
                    }
                    onEditingFinished: {
                        signerSettings.limitAutoSignTime = text
                    }
                }
            }
        }
    }

    function storeSettings() {
        if (signerSettings.limitAutoSignXbt !== limitAutoSignXbt.text) {
            if (limitAutoSignXbt.text !== qsTr("Unlimited")) {
                signerSettings.limitAutoSignXbt = limitAutoSignXbt.text
            }
        }

        signerSettings.limitAutoSignTime = limitAutoSignTime.text
    }
}
