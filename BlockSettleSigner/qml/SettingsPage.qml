/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import Qt.labs.platform 1.1

import com.blocksettle.QmlFactory 1.0

import "StyledControls"
import "BsControls"
import "BsStyles"
import "BsDialogs"
import "js/helper.js" as JsHelper

Item {
    Rectangle {
        id: rectHelp

        width: labelHelp.width
        height: labelHelp.height
        z: 1
        color: "black"
        // visible: twoway_help_mouse_area.containsMouse
        visible: false

        CustomLabel {
            id: labelHelp
            text: qsTr("Two way authentication")
            padding: 5

            Component.onCompleted: {
                if (labelHelp.paintedWidth > 500) {
                    labelHelp.width = 500
                }
            }
        }
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

            // Does not work anymore
            // New message type to headless signer would be needed
//            RowLayout {
//                id: row3
//                Layout.topMargin: 5
//                Layout.fillWidth: true
//                Layout.rightMargin: 10
//                Layout.leftMargin: 10

//                CustomLabel {
//                    text: qsTr("Wallets directory")
//                    Layout.minimumWidth: 125
//                    Layout.preferredWidth: 125
//                    Layout.maximumWidth: 125
//                }

//                CustomLabel {
//                    Layout.alignment: Qt.AlignLeft
//                    Layout.fillWidth: true
//                    wrapMode: Text.Wrap
//                    text: signerSettings.walletsDir
//                    color: BSStyle.textColor

//                }

//                CustomButton {
//                    text: qsTr("Select")
//                    Layout.minimumWidth: 80
//                    Layout.preferredWidth: 80
//                    Layout.maximumWidth: 80
//                    Layout.maximumHeight: 26
//                    Layout.rightMargin: 6
//                    onClicked: {
//                        if (!ldrWalletsDirDlg.item) {
//                            ldrWalletsDirDlg.active = true
//                        }
//                        ldrWalletsDirDlg.startFromFolder = Qt.resolvedUrl(signerSettings.walletsDir)
//                        ldrWalletsDirDlg.item.bsAccepted.connect(function() {
//                            signerSettings.walletsDir = ldrWalletsDirDlg.dir
//                        })
//                        ldrWalletsDirDlg.item.open();
//                    }
//                }
//            }

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
//                onClicked: {
//                    gridNetwork.state = checked ? "normal" : "hidden"
//                    highlighted = !checked
//                    down = checked
//                }
            }

            SettingsGrid {
                id: gridNetwork

                CustomLabel {
                    text: qsTr("Accept connections from")
                    Layout.fillWidth: true
                }
                CustomTextInput {
                    placeholderText: "0.0.0.0/0"

                    Layout.minimumWidth: 440
                    Layout.preferredWidth: 440
                    Layout.maximumWidth: 440
                    Layout.alignment: Qt.AlignRight

                    Layout.rightMargin: 6
                    text: signerSettings.acceptFrom
                    selectByMouse: true
                    id: acceptFrom
                    onEditingFinished: {
                        signerSettings.acceptFrom = text
                    }

                    ToolTip.visible: hovered
                    ToolTip.delay: 1000
                    ToolTip.timeout: 10000
                    ToolTip.text: qsTr("Set IP address or subnet (in CIDR notation) that will be able to connect to the signer.\n\nExamples:\nLocal host only: 127.0.0.1\nFrom some LAN: 192.168.1.0/24\nAnybody (default): 0.0.0.0/0")
                }

                CustomLabel {
                    text: qsTr("Listening port")
                    Layout.fillWidth: true
                }
                CustomTextInput {
                    placeholderText: "23456"

                    Layout.minimumWidth: 440
                    Layout.preferredWidth: 440
                    Layout.maximumWidth: 440
                    Layout.alignment: Qt.AlignRight

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

            RowLayout {
                id: row5
                Layout.topMargin: 5
                Layout.fillWidth: true
                Layout.rightMargin: 10
                Layout.leftMargin: 10

                CustomLabel {
                    text: qsTr("Signer ID Key")
                    Layout.minimumWidth: 125
                    Layout.preferredWidth: 125
                    Layout.maximumWidth: 125
                }
                CustomLabel {
                    Layout.fillWidth: true
                }
                CustomLabel {
                    Layout.alignment: Qt.AlignRight
                    Layout.rightMargin: 6
                    wrapMode: Text.Wrap
                    text: qmlFactory.headlessPubKey
                    color: BSStyle.textColor
                }
            }

            RowLayout {
                id: row51
                Layout.topMargin: 5
                Layout.fillWidth: true
                Layout.rightMargin: 10
                Layout.leftMargin: 10

                CustomLabel {
                    Layout.fillWidth: true
                }
                CustomButton {
                    id: btnHeadlessKeyCopy
                    text: qsTr("Copy")
                    Layout.minimumWidth: 150
                    Layout.preferredWidth: 150
                    Layout.maximumWidth: 150
                    Layout.maximumHeight: 22
                    Layout.rightMargin: 6

                    onClicked: {
                        qmlFactory.setClipboard(qmlFactory.headlessPubKey)
                        btnHeadlessKeyCopy.text = qsTr("Copied")
                        enabled = false
                        copiedTimer.start()
                    }

                    Timer {
                       id: copiedTimer
                       repeat: false
                       interval: 1000
                       onTriggered: {
                           btnHeadlessKeyCopy.enabled = true
                           btnHeadlessKeyCopy.text = qsTr("Copy")
                       }
                    }
                }
                CustomButton {
                    text: qsTr("Export")
                    Layout.minimumWidth: 150
                    Layout.preferredWidth: 150
                    Layout.maximumWidth: 150
                    Layout.maximumHeight: 22
                    Layout.rightMargin: 6
                    onClicked: exportHeadlessPubKeyDlg.open()

                    FileDialog {
                        id: exportHeadlessPubKeyDlg
                        title: "Save Signer ID Key"

                        currentFile: StandardPaths.writableLocation(StandardPaths.DocumentsLocation) + "/signer_id_key.pub"
                        folder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
                        fileMode: FileDialog.SaveFile
                        nameFilters: [ "Key files (*.pub)", "All files (*)" ]

                        onAccepted: {
                            var zmqPubKey = qmlFactory.headlessPubKey
                            JsHelper.saveTextFile(file, zmqPubKey)
                        }
                    }
                }
            }

            CustomHeader {
                id: terminalKeys
                text: qsTr("Terminal Settings")
                checkable: true
                checked: true
                down: true
                Layout.preferredHeight: 25
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                Layout.topMargin: 10
            }

            RowLayout {
                Layout.topMargin: 5
                Layout.fillWidth: true
                Layout.rightMargin: 10
                Layout.leftMargin: 10

                CustomLabel {
                    text: qsTr("Terminal ID Key Authentication")
                }

//                Image {
//                    id: twoway_help_image
//                    Layout.maximumWidth: 10
//                    Layout.maximumHeight: 10

//                    source: "qrc:/resources/notification_info.png"
//                    MouseArea {
//                        id: twoway_help_mouse_area
//                        anchors.fill: parent
//                        hoverEnabled: true
//                        onHoveredChanged: {
//                            rectHelp.x = rectHelp.mapFromItem(twoway_help_image, 0, 0).x + 15
//                            rectHelp.y = rectHelp.mapFromItem(twoway_help_image, 0, 0).y - 10
//                        }
//                    }
//                }

//                CustomLabel {
//                    id: twoway_help_label
//                    visible: twoway_help_mouse_area.containsMouse
//                    Layout.fillWidth: true
//                }

                CustomLabel {
                    Layout.fillWidth: true
                }

                CustomSwitch {
                    Layout.alignment: Qt.AlignRight
                    checked: signerSettings.twoWaySignerAuth
                    onClicked: {
                        // Allow enable two way without additional prompts
                        if (checked) {
                            signerSettings.twoWaySignerAuth = true
                            return
                        }

                        var dlg = JsHelper.messageBox(BSMessageBox.Type.Question, "Two-way Authentication"
                           , "Disable two-way authentication?"
                           , "BlockSettle strongly discourages disabling signer side authentication of incoming connections. Do you wish to continue?")
                        dlg.labelText.color = BSStyle.dialogTitleOrangeColor
                        dlg.bsAccepted.connect(function() {
                            signerSettings.twoWaySignerAuth = false
                        })
                        dlg.bsRejected.connect(function() {
                            checked = true
                        })
                    }
                }
            }

            RowLayout {
                id: rowManageKeys
                Layout.topMargin: 5
                Layout.fillWidth: true
                Layout.rightMargin: 10
                Layout.leftMargin: 10

                CustomLabel {
                    text: qsTr("Terminals ID Keys")
                    Layout.minimumWidth: 125
                    Layout.preferredWidth: 125
                    Layout.maximumWidth: 125
                }

                CustomLabel {
                    Layout.fillWidth: true
                }

                CustomButton {
                    text: qsTr("Manage")
                    Layout.minimumWidth: 150
                    Layout.preferredWidth: 150
                    Layout.maximumWidth: 150
                    Layout.maximumHeight: 22
                    Layout.rightMargin: 6
                    onClicked: {
                        var dlgTerminals = Qt.createComponent("BsDialogs/TerminalKeysDialog.qml").createObject(mainWindow)
                        dlgTerminals.open()
                    }
                }
            }

            CustomHeader {
                text: qsTr("Security Settings")
                checkable: true
                checked: true
                down: true
                Layout.preferredHeight: 25
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                Layout.topMargin: 10
            }

            RowLayout {
                Layout.topMargin: 5
                Layout.fillWidth: true
                Layout.rightMargin: 10
                Layout.leftMargin: 10

                CustomLabel {
                    text: qsTr("Master Password")
                    Layout.minimumWidth: 125
                    Layout.preferredWidth: 125
                    Layout.maximumWidth: 125
                }

                CustomLabel {
                    Layout.fillWidth: true
                }

                CustomButton {
                    text: qsTr("Change")
                    Layout.minimumWidth: 150
                    Layout.preferredWidth: 150
                    Layout.maximumWidth: 150
                    Layout.maximumHeight: 22
                    Layout.rightMargin: 6
                    onClicked: {
                        var onControlPasswordChanged = function(success, errorMsg){
                            if (success) {
                                JsHelper.messageBox(BSMessageBox.Type.Success
                                    , qsTr("Master Password"), qsTr("Change Master Password succeed"))

                            } else {
                                JsHelper.messageBox(BSMessageBox.Type.Critical
                                    , qsTr("Master Password"), qsTr("Change Master Password failed: \n") + errorMsg)
                            }
                        }
                        var onControlPasswordFinished = function(dlg, oldPassword, newPassword){
                            walletsProxy.changeControlPassword(oldPassword, newPassword, onControlPasswordChanged)
                        }
                        JsHelper.createControlPasswordDialog(onControlPasswordFinished, qmlFactory.controlPasswordStatus())
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

        if (signerSettings.limitManualXbt !== limitManualXbt.text) {
            if (limitManualXbt.text !== qsTr("Unlimited")) {
                signerSettings.limitManualXbt = limitManualXbt.text
            }
        }
        if (signerSettings.listenPort !== listenPort.text) {
            signerSettings.listenPort = listenPort.text
        }
        if (signerSettings.acceptFrom !== acceptFrom.text) {
            signerSettings.acceptFrom = acceptFrom.text
        }
    }
}
