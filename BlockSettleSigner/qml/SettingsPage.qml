import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

import "StyledControls"
import "BsStyles"
import "BsDialogs"

Item {
    DirSelectionDialog {
        id: ldrWalletsDirDlg
        title: qsTr("Select wallets directory")
    }

    ScrollView {
        anchors.fill: parent
        id: settingsView
        clip: true


        ColumnLayout {
            width: parent.parent.width
            spacing: 5

            CustomHeader {
                text: qsTr("General Settings")
                font.pixelSize: 14
                height: 25
                checkable: true
                checked: true
                down: true
                Layout.fillWidth: true
                Layout.preferredHeight: 25
                Layout.topMargin: 5
                Layout.leftMargin: 10
                Layout.rightMargin: 10

                onClicked: {
                    gridGeneral.state = checked ? "normal" : "hidden"
                    highlighted = !checked
                    down = checked
                }
            }

            RowLayout {
                id: row1
                spacing: 5
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10

                CustomLabel {
                    text: qsTr("Online mode")
                    Layout.fillWidth: true
                }

                CustomSwitch {
                    Layout.alignment: Qt.AlignRight
                    text: signerStatus.socketOk ? "" : qsTr("Failed to bind")
                    checked: !signerStatus.offline
                    onClicked: {
                        signerSettings.offline = !checked
                    }
                }
            }

            RowLayout {
                id: row2
                spacing: 5
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10

                CustomLabel {
                    text: qsTr("TestNet")
                    Layout.fillWidth: true
                }

                CustomSwitch {
                    Layout.alignment: Qt.AlignRight
                    checked: signerSettings.testNet
                    onClicked: {
                        signerSettings.testNet = checked
                    }
                }
            }

            RowLayout {
                id: row3
                Layout.topMargin: 5
                Layout.fillWidth: true
                Layout.rightMargin: 10
                Layout.leftMargin: 10

                CustomLabel {
                    text: qsTr("Wallets directory")
                    Layout.minimumWidth: 125
                    Layout.preferredWidth: 125
                    Layout.maximumWidth: 125
                }

                CustomLabel {
                    Layout.alignment: Qt.AlignLeft
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: signerSettings.walletsDir
                    color: BSStyle.textColor

                }

                CustomButton {
                    text: qsTr("Select")
                    Layout.minimumWidth: 80
                    Layout.preferredWidth: 80
                    Layout.maximumWidth: 80
                    Layout.maximumHeight: 26
                    Layout.rightMargin: 6
                    onClicked: {
                        if (!ldrWalletsDirDlg.item) {
                            ldrWalletsDirDlg.active = true
                        }
                        ldrWalletsDirDlg.startFromFolder = Qt.resolvedUrl(signerSettings.walletsDir)
                        ldrWalletsDirDlg.item.accepted.connect(function() {
                            signerSettings.walletsDir = ldrWalletsDirDlg.dir
                        })
                        ldrWalletsDirDlg.item.open();
                    }
                }
            }

            CustomHeader {
                id: btnNetwork
                text: qsTr("Network Settings")
                checkable: true
                checked: true
                down: true
                Layout.preferredHeight: 25
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                Layout.topMargin: 10
                onClicked: {
                    gridNetwork.state = checked ? "normal" : "hidden"
                    highlighted = !checked
                    down = checked
                }
            }

            SettingsGrid {
                id: gridNetwork

                CustomLabel {
                    text: qsTr("Connection password")
                    Layout.minimumWidth: 125
                    Layout.preferredWidth: 125
                    Layout.maximumWidth: 125
                }
                CustomTextInput {
                    placeholderText: qsTr("Password")
                    echoMode: TextField.Password
                    text: signerSettings.password
                    Layout.fillWidth: true
                    Layout.rightMargin: 6
                    selectByMouse: true
                    id: password
                    onEditingFinished: {
                        signerSettings.password = text
                    }
                }

                CustomLabel {
                    text: qsTr("Listen IP address")
                    Layout.minimumWidth: 125
                    Layout.preferredWidth: 125
                    Layout.maximumWidth: 125
                }
                CustomTextInput {
                    placeholderText: "0.0.0.0"
                    Layout.fillWidth: true
                    Layout.rightMargin: 6
                    text: signerSettings.listenAddress
                    selectByMouse: true
                    id: listenAddress
                    validator: RegExpValidator {
                        regExp: /^((?:[0-1]?[0-9]?[0-9]?|2[0-4][0-9]|25[0-5])\.){0,3}(?:[0-1]?[0-9]?[0-9]|2[0-4][0-9]|25[0-5])$/
                    }
                    onEditingFinished: {
                        signerSettings.listenAddress = text
                    }
                }

                CustomLabel {
                    text: qsTr("Listening port")
                    Layout.minimumWidth: 125
                    Layout.preferredWidth: 125
                    Layout.maximumWidth: 125
                }
                CustomTextInput {
                    placeholderText: "23456"
                    Layout.fillWidth: true
                    Layout.rightMargin: 6
                    text: signerSettings.listenPort
                    selectByMouse: true
                    id: listenPort
                    validator: IntValidator {
                        bottom: 0
                        top: 65535
                    }
                    onEditingFinished: {
                        signerSettings.listenPort = text
                    }
                }
            }

            CustomHeader {
                visible: false
                text: qsTr("Limits")
                font.pixelSize: 14
                checkable: true
                checked: true
                down: true
                Layout.fillWidth: true
                Layout.preferredHeight: 25
                Layout.leftMargin: 5
                Layout.rightMargin: 5
                onClicked: {
                    gridLimits.state = checked ? "normal" : "hidden"
                    highlighted = !checked
                    down = checked
                }
            }

            SettingsGrid {
                visible: false
                id: gridLimits

                CustomLabel {
                    text: qsTr("Manual XBT spend limit")

                }
                CustomTextInput {
                    Layout.fillWidth: true
                    text: signerSettings.manualSignUnlimited ? qsTr("Unlimited") : signerSettings.limitManualXbt
                    selectByMouse: true
                    id: limitManualXbt
                    validator: RegExpValidator {
                        regExp: /^[0-9]*\.?[0-9]*$/
                    }
                    onEditingFinished: {
                        if (text !== qsTr("Unlimited")) {
                            signerSettings.limitManualXbt = text
                        }
                    }
                }

                CustomLabel {
                    text: qsTr("Interval for keeping wallet password in memory")
                }
                CustomTextInput {
                    Layout.fillWidth: true
                    placeholderText: "30s or 5min"
                    text: signerSettings.limitManualPwKeep
                    selectByMouse: true
                    id: limitManualPwKeep
                    validator: RegExpValidator {
                        regExp: /^(?:\d+(h|hour|m|min|minute|s|sec|second)?\s*)*$/
                    }
                    onEditingFinished: {
                        signerSettings.limitManualPwKeep = text
                    }
                }
            }
        }
    }

    function storeSettings() {
        signerSettings.limitManualPwKeep = limitManualPwKeep.text

        if (signerSettings.limitManualXbt != limitManualXbt.text) {
            if (limitManualXbt.text !== qsTr("Unlimited")) {
                signerSettings.limitManualXbt = limitManualXbt.text
            }
        }
        if (signerSettings.listenPort !== listenPort.text) {
            signerSettings.listenPort = listenPort.text
        }
        if (signerSettings.listenAddress !== listenAddress.text) {
            signerSettings.listenAddress = listenAddress.text
        }
        if (signerSettings.password !== password.text) {
            signerSettings.password = password.text
        }
    }
}
