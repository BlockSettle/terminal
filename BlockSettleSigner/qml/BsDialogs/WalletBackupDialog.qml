/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2
import Qt.labs.platform 1.1

import com.blocksettle.AutheIDClient 1.0
import com.blocksettle.AuthSignWalletObject 1.0
import com.blocksettle.WalletInfo 1.0
import com.blocksettle.QSeed 1.0
import com.blocksettle.QPasswordData 1.0
import com.blocksettle.WalletsProxy 1.0

import "../StyledControls"
import "../BsControls"
import "../js/helper.js" as JsHelper

CustomTitleDialogWindow {
    id: root

    property AuthSignWalletObject authSign: AuthSignWalletObject {}
    property WalletInfo walletInfo: WalletInfo {}

    property string targetFile: qmlAppObj.getUrlPath(StandardPaths.writableLocation(StandardPaths.DocumentsLocation) + "/" + backupFileName)

    property string backupFileExt: "." + (isPrintable ? "pdf" : "wdb")

    // suggested new file names
    property string backupFileName: fullBackupMode
                                    ? "BlockSettle_" + walletInfo.walletId + backupFileExt
                                    : "BlockSettle_" + walletInfo.walletId + "_WatchingOnly.lmdb"

    property bool   isPrintable: false
    property bool   acceptable: (walletInfo.encType === QPasswordData.Unencrypted)
                                    || walletInfo.encType === QPasswordData.Auth
                                    || walletDetailsFrame.password.length

    property bool fullBackupMode: tabBar.currentIndex === 0
    property bool woBackupAllowed: true

    property bool isWoWallet: false

    width: 400
    height: 495

    title: qsTr("Export")
    rejectable: true
    onEnterPressed: {
        if (btnAccept.enabled) btnAccept.onClicked()
    }

    onWalletInfoChanged: {
        // need to update object since bindings working only for basic types
        walletDetailsFrame.walletInfo = walletInfo
        if (walletsProxy.isWatchingOnlyWallet(walletInfo.rootId)) {
            isWoWallet = true
            tabBar.currentIndex = 1
        }
    }

    cContentItem: ColumnLayout {
        id: mainLayout
        spacing: 10

        TabBar {
            id: tabBar
            spacing: 0
            leftPadding: 1
            rightPadding: 1
            height: 35

            Layout.fillWidth: true
            position: TabBar.Header

            background: Rectangle {
                anchors.fill: parent
                color: "transparent"
            }

            CustomTabButton {
                id: fullBackupTabButton
                text: "Full"
                cText.font.capitalization: Font.MixedCase
                implicitHeight: 35
                enabled: !isWoWallet
            }
            CustomTabButton {
                id: woBackupTabButton
                text: "Watch-Only"
                cText.font.capitalization: Font.MixedCase
                implicitHeight: 35
                enabled: woBackupAllowed
            }
        }

        BSWalletDetailsFrame {
            id: walletDetailsFrame
            Layout.fillHeight: false

            walletInfo: walletInfo
            inputsWidth: 250
            onPasswordEntered:{
                if (btnAccept.enabled) btnAccept.onClicked()
            }
        }

        CustomHeader {
            text: fullBackupMode ? qsTr("Backup Wallet") : qsTr("Export Watching-Only Copy")
            Layout.fillWidth: true
            Layout.preferredHeight: 25
            Layout.topMargin: 5
            Layout.leftMargin: 10
            Layout.rightMargin: 10
        }

        RowLayout {
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            visible: fullBackupMode

            CustomLabel {
                Layout.preferredWidth: 110
                text: qsTr("Backup Type")
                Layout.alignment: Qt.AlignTop
            }

            RowLayout {
                Layout.fillWidth: true
                CustomRadioButton {
                    text: qsTr("Digital Backup")
                    checked: !isPrintable
                    onClicked: {
                        isPrintable = false
                    }
                }
                CustomRadioButton {
                    text: qsTr("Paper Backup")
                    checked: isPrintable
                    onClicked: {
                        isPrintable = true
                    }
                }
            }
        }
        RowLayout {
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomLabel {
                Layout.preferredWidth: 110
                text: qsTr("Backup file")
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
            }
            CustomLabelValue {
                text: targetFile
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.preferredWidth: 300
            }
        }
        RowLayout {
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomButton {
                text: qsTr("Select Target Dir")
                Layout.preferredWidth: 80
                Layout.maximumHeight: 25
                Layout.leftMargin: 110 + 5

                FileDialog {
                    id: fileDialog
                    currentFile: StandardPaths.writableLocation(StandardPaths.DocumentsLocation) + "/" + backupFileName
                    folder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
                    fileMode: FileDialog.SaveFile

                    onAccepted: {
                        targetFile = qmlAppObj.getUrlPath(file)
                    }
                }

                onClicked: {
                    fileDialog.open()
                }
            }
        }

        Rectangle {
            Layout.fillHeight: true
        }
    }

    cFooterItem: RowLayout {
        CustomButtonBar {
            Layout.fillWidth: true
            id: rowButtons

            CustomButton {
                text: qsTr("Cancel")
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                onClicked: {
                    rejectAnimated()
                }
            }

            CustomButton {
                id: btnAccept
                primary: true
                enabled: acceptable
                text: qsTr("CONFIRM")
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                onClicked: {
                    var exportCallback = function(success, errorMsg) {
                        if (success) {
                            var mb = JsHelper.messageBox(BSMessageBox.Type.Success
                                , qsTr("Wallet Export")
                                , qsTr("%1Wallet successfully exported")
                                    .arg(fullBackupMode ? "" : "Watching-Only ")
                                , qsTr("Wallet Name: %1\nWallet ID: %2\nBackup location: '%3'")
                                    .arg(walletInfo.name)
                                    .arg(walletInfo.walletId)
                                    .arg(targetFile))
                            mb.bsAccepted.connect(function(){ acceptAnimated() })
                        } else {
                            JsHelper.messageBox(BSMessageBox.Type.Critical
                                , qsTr("Error")
                                , qsTr("%1Wallet export failed")
                                    .arg(fullBackupMode ? "" : "Watching-Only ")
                                , errorMsg)
                        }
                    }

                    if (walletInfo.encType === QPasswordData.Password || walletInfo.encType === QPasswordData.Unencrypted) {
                        var passwordData = qmlFactory.createPasswordData()
                        passwordData.textPassword = walletDetailsFrame.password

                        if (fullBackupMode) {
                            rc = walletsProxy.backupPrivateKey(walletInfo.walletId
                               , targetFile, isPrintable
                               , passwordData, exportCallback)
                            if (! rc) {
                                JsHelper.messageBox(BSMessageBox.Type.Critical
                                    , qsTr("Error")
                                    , qsTr("Wallet export failed")
                                    , qsTr("Internal error"))
                            }
                        }
                        else {
                            walletsProxy.exportWatchingOnly(walletInfo.walletId
                               , targetFile, passwordData
                               , exportCallback)
                        }
                    }
                    else if (walletInfo.encType === QPasswordData.Auth) {
                        JsHelper.requesteIdAuth(AutheIDClient.BackupWallet, walletInfo
                            , function(passwordData){
                                if (fullBackupMode) {
                                    walletsProxy.backupPrivateKey(walletInfo.walletId
                                       , targetFile, isPrintable
                                       , passwordData, exportCallback)
                                }
                                else {
                                    walletsProxy.exportWatchingOnly(walletInfo.walletId
                                       , targetFile, passwordData
                                       , exportCallback)
                                }
                        })
                    }
                }
            }
        }
    }
}
