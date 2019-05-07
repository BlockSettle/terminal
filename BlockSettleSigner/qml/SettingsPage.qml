import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import QtQuick.Dialogs 1.2
import com.blocksettle.QmlFactory 1.0

import "StyledControls"
import "BsStyles"
import "BsDialogs"
import "js/helper.js" as JsHelper

Item {
    DirSelectionDialog {
        id: ldrWalletsDirDlg
        title: qsTr("Select wallets directory")
    }

    Component.onCompleted: {
        qmlFactory.requestHeadlessPubKey()
    }

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

            RowLayout {
                Layout.topMargin: 5
                Layout.fillWidth: true
                Layout.rightMargin: 10
                Layout.leftMargin: 10

                CustomLabel {
                    text: qsTr("AuthKey broadcast")
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
                    checked: signerSettings.twoWayAuth
                    onClicked: {
                        signerSettings.twoWayAuth = checked
                    }
                }
            }


//            RowLayout {
//                id: row4
//                Layout.topMargin: 5
//                Layout.fillWidth: true
//                Layout.rightMargin: 10
//                Layout.leftMargin: 10

//                CustomLabel {
//                    text: qsTr("Signer Private Key")
//                    Layout.minimumWidth: 125
//                    Layout.preferredWidth: 125
//                    Layout.maximumWidth: 125
//                }

//                CustomLabel {
//                    Layout.alignment: Qt.AlignLeft
//                    Layout.fillWidth: true
//                    wrapMode: Text.Wrap
//                    text: signerSettings.signerPrvKey
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
//                        zmqPrivKeyDlg.folder = "file:///" + JsHelper.folderOfFile(signerSettings.signerPrvKey)
//                        zmqPrivKeyDlg.open()
//                        zmqPrivKeyDlg.bsAccepted.connect(function(){
//                            signerSettings.signerPrvKey = JsHelper.fileUrlToPath(zmqPrivKeyDlg.fileUrl)
//                        })
//                    }
//                    FileDialog {
//                        id: zmqPrivKeyDlg
//                        visible: false
//                        title: "Select Signer Private Key"
//                        selectFolder: false
//                    }
//                }
//            }

            RowLayout {
                id: row5
                Layout.topMargin: 5
                Layout.fillWidth: true
                Layout.rightMargin: 10
                Layout.leftMargin: 10

                CustomLabel {
                    text: qsTr("Signer Public Key")
                    Layout.minimumWidth: 125
                    Layout.preferredWidth: 125
                    Layout.maximumWidth: 125
                }

                CustomLabel {
                    Layout.alignment: Qt.AlignLeft
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: qmlFactory.headlessPubKey
                    color: BSStyle.textColor

                }
                CustomButton {
                    id: btnZmqKeyCopy
                    text: qsTr("Copy")
                    Layout.minimumWidth: 80
                    Layout.preferredWidth: 80
                    Layout.maximumWidth: 80
                    Layout.maximumHeight: 26
                    Layout.rightMargin: 6

                    onClicked: {
                        qmlFactory.setClipboard(qmlFactory.headlessPubKey)
                        btnZmqKeyCopy.text = qsTr("Copied")
                        enabled = false
                        copiedTimer.start()
                    }

                    Timer {
                       id: copiedTimer
                       repeat: false
                       interval: 1000
                       onTriggered: {
                           btnZmqKeyCopy.enabled = true
                           btnZmqKeyCopy.text = qsTr("Copy")
                       }
                    }
                }
                CustomButton {
                    text: qsTr("Export")
                    Layout.minimumWidth: 80
                    Layout.preferredWidth: 80
                    Layout.maximumWidth: 80
                    Layout.maximumHeight: 26
                    Layout.rightMargin: 6
                    onClicked: {
                        //exportSignerPubKeyDlg.folder = "file:///" + JsHelper.folderOfFile(signerSettings.signerPubKey)
                        //exportSignerPubKeyDlg.folder = shortcuts.home
                        exportSignerPubKeyDlg.open()
                        exportSignerPubKeyDlg.accepted.connect(function(){
                            var zmqPubKey = qmlFactory.headlessPubKey
                            JsHelper.saveTextFile(exportSignerPubKeyDlg.fileUrl, zmqPubKey)
                        })
                    }
                    FileDialog {
                        id: exportSignerPubKeyDlg
                        visible: false
                        title: "Save Signer Public Key"
                        selectFolder: false
                        selectExisting: false
                        nameFilters: [ "Key files (*.pub)", "All files (*)" ]
                        selectedNameFilter: "*.pub"
                        folder: shortcuts.documents
                    }
                }
            }

            SettingsGrid {
                id: gridNetwork

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
                id: rowManageKeys
                Layout.topMargin: 5
                Layout.fillWidth: true
                Layout.rightMargin: 10
                Layout.leftMargin: 10

                CustomLabel {
                    text: qsTr("Terminals keys")
                    Layout.minimumWidth: 125
                    Layout.preferredWidth: 125
                    Layout.maximumWidth: 125
                }

                CustomLabel {
                    Layout.fillWidth: true
                }

                CustomButton {
                    text: qsTr("Manage")
                    Layout.minimumWidth: 80
                    Layout.preferredWidth: 80
                    Layout.maximumWidth: 80
                    Layout.maximumHeight: 26
                    Layout.rightMargin: 6
                    onClicked: {
                        var dlgTerminals = Qt.createComponent("BsDialogs/TerminalKeysDialog.qml").createObject(mainWindow)
                        dlgTerminals.open()
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
        if (signerSettings.listenAddress !== listenAddress.text) {
            signerSettings.listenAddress = listenAddress.text
        }
    }
}
