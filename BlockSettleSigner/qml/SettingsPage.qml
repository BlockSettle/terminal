import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Controls.Styles 1.4
import QtQuick.Layouts 1.3

Item {
    DirSelectionDialog {
        id:     ldrWalletsDirDlg
        title:  qsTr("Select wallets directory")
    }

    ScrollView {
        anchors.fill: parent
        id: settingsView
        clip:   true


        ColumnLayout {
            width:  parent.parent.width
            spacing: 5

            CustomHeader {
                text:   qsTr("General Settings:")
                font.pixelSize: 14
                height: 25
                checkable: true
                checked:   true
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
                    text:   qsTr("Connection mode:")
                    Layout.minimumWidth: 125
                    Layout.preferredWidth: 125
                    Layout.maximumWidth: 125
                }
                CustomCheckBox {
                    Layout.fillWidth: true
                    text: signerStatus.socketOk ? "" : qsTr("Failed to bind")
                    checked:    !signerParams.offline
                    onCheckStateChanged: {
                        signerParams.offline = !checked
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
                    text:   qsTr("Bitcoin network type TestNet:")
                    Layout.minimumWidth: 125
                    Layout.preferredWidth: 125
                    Layout.maximumWidth: 125
                }
                CustomCheckBox {
                    Layout.fillWidth: true
                    text: checked ? qsTr("Enabled") : qsTr("Disabled")
                    checked:    signerParams.testNet
                    onCheckStateChanged: {
                        signerParams.testNet = checked
                    }
                }
            }

            ColumnLayout {
                id: col2
                spacing: 10
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10

                RowLayout {
                    id: row3
                    spacing: 5
                    Layout.fillWidth: true


                    CustomLabel {
                        text:   qsTr("Wallets directory:")
                        Layout.minimumWidth: 125
                        Layout.preferredWidth: 125
                        Layout.maximumWidth: 125
                        Layout.fillWidth: true
                    }

                    CustomLabel {
                        Layout.alignment: Qt.AlignRight
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        text:   signerParams.walletsDir
                        color: "#ffffff"

                    }
                }

                RowLayout {
                    id: row4
                    spacing: 10
                    Layout.fillWidth: true

                    CustomButton {
                        text:   qsTr("Select")
                        Layout.minimumWidth: 80
                        Layout.preferredWidth: 80
                        Layout.maximumWidth: 80
                        Layout.maximumHeight: 25
                        Layout.leftMargin: 110 + 5
                        onClicked: {
                            if (!ldrWalletsDirDlg.item) {
                                ldrWalletsDirDlg.active = true
                            }
                            ldrWalletsDirDlg.startFromFolder = Qt.resolvedUrl(signerParams.walletsDir)
                            ldrWalletsDirDlg.item.accepted.connect(function() {
                                signerParams.walletsDir = ldrWalletsDirDlg.dir
                            })
                            ldrWalletsDirDlg.item.open();
                        }
                    }
                }
            }

            CustomHeader {
                id: btnNetwork
                text:   qsTr("Network Settings:")
                checkable: true
                checked:   true
                down: true
                Layout.preferredHeight: 25
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                onClicked:  {
                    gridNetwork.state = checked ? "normal" : "hidden"
                    highlighted = !checked
                    down = checked
                }
            }

            SettingsGrid {
                id: gridNetwork

                CustomLabel {
                    text:   qsTr("Connection password:")
                }
                CustomTextInput {
                    placeholderText: qsTr("Password")
                    echoMode:   TextField.Password
                    text:       signerParams.password
                    Layout.fillWidth: true
                    selectByMouse: true
                    id: password
                    onEditingFinished: {
                        signerParams.password = text
                    }
                }

                CustomLabel {
                    text:   qsTr("Listen IP address:")
                }
                CustomTextInput {
                    placeholderText: "0.0.0.0"
                    Layout.fillWidth: true
                    text:   signerParams.listenAddress
                    selectByMouse: true
                    id: listenAddress
                    validator: RegExpValidator {
                        regExp: /^\.?((?:[0-1]?[0-9]?[0-9]|2[0-4][0-9]|25[0-5])\.){0,3}(?:[0-1]?[0-9]?[0-9]|2[0-4][0-9]|25[0-5])$/
                    }
                    onEditingFinished: {
                        signerParams.listenAddress = text
                    }
                }

                CustomLabel {
                    text:   qsTr("Listening port:")
                }
                CustomTextInput {
                    placeholderText: "23456"
                    Layout.fillWidth: true
                    text:   signerParams.listenPort
                    selectByMouse: true
                    id: listenPort
                    validator: IntValidator {
                        bottom: 0
                        top: 65535
                    }
                    onEditingFinished: {
                        signerParams.listenPort = text
                    }
                }
            }

            CustomHeader {
                visible: false
                text:   qsTr("Limits:")
                font.pixelSize: 14
                checkable: true
                checked:   true
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
                    text:   qsTr("Manual XBT spend limit:")

                }
                CustomTextInput {
                    Layout.fillWidth: true
                    text:   signerParams.manualSignUnlimited ? qsTr("Unlimited") : signerParams.limitManualXbt
                    selectByMouse: true
                    id: limitManualXbt
                    onEditingFinished: {
                        if (text !== qsTr("Unlimited")) {
                            signerParams.limitManualXbt = text
                        }
                    }
                }

                CustomLabel {
                    text:   qsTr("Interval for keeping wallet password in memory:")
                }
                CustomTextInput {
                    Layout.fillWidth: true
                    placeholderText: "30s or 5min"
                    text:   signerParams.limitManualPwKeep
                    selectByMouse: true
                    id: limitManualPwKeep
                    onEditingFinished: {
                        signerParams.limitManualPwKeep = text
                    }
                }
            }
        }
    }

    function storeSettings() {
        if (signerParams.limitManualPwKeep !== limitManualPwKeep.text) {
            signerParams.limitManualPwKeep = limitManualPwKeep.text
        }
        if (signerParams.limitManualXbt != limitManualXbt.text) {
            if (limitManualXbt.text !== qsTr("Unlimited")) {
                signerParams.limitManualXbt = limitManualXbt.text
            }
        }
        if (signerParams.listenPort !== listenPort.text) {
            signerParams.listenPort = listenPort.text
        }
        if (signerParams.listenAddress !== listenAddress.text) {
            signerParams.listenAddress = listenAddress.text
        }
        if (signerParams.password !== password.text) {
            signerParams.password = password.text
        }
    }
}
