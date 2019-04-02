import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2

import com.blocksettle.AutheIDClient 1.0
import com.blocksettle.AuthSignWalletObject 1.0
import com.blocksettle.WalletInfo 1.0
import com.blocksettle.QSeed 1.0
import com.blocksettle.QPasswordData 1.0
import com.blocksettle.NsWallet.namespace 1.0

import "../StyledControls"
import "../BsControls"
import "../js/helper.js" as JsHelper

CustomTitleDialogWindow {
    id: root

    property WalletInfo walletInfo: WalletInfo{}
    property string targetDir
    property string backupFileExt: "." + (isPrintable ? "pdf" : "wdb")
    property string backupFileName: fullBackupMode
                                    ? "backup_wallet_" + walletInfo.name + "_" + walletInfo.walletId + backupFileExt
                                    : "wo_backup_wallet_" + walletInfo.name + "_" + walletInfo.walletId + ".lmdb"
    property bool   isPrintable: false
    property bool   acceptable: (walletInfo.encType === NsWallet.Unencrypted)
                                    || walletInfo.encType === NsWallet.Auth
                                    || walletDetailsFrame.password.length

    property bool fullBackupMode: tabBar.currentIndex === 0
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
    }

    cContentItem: ColumnLayout {
        id: mainLayout
        spacing: 10

        TabBar {
            id: tabBar
            spacing: 2
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
            }
            CustomTabButton {
                id: woBackupTabButton
                text: "Watch-Only"
                cText.font.capitalization: Font.MixedCase
                implicitHeight: 35
            }
        }

        BSWalletDetailsFrame {
            id: walletDetailsFrame
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
                text: qsTr("%1/%2").arg(targetDir).arg(backupFileName)
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
                onClicked: {
                    if (!ldrDirDlg.item) {
                        ldrDirDlg.active = true
                    }
                    ldrDirDlg.startFromFolder = targetDir
                    ldrDirDlg.item.bsAccepted.connect(function() {
                        targetDir = ldrDirDlg.dir
                    })
                    ldrDirDlg.item.open();
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

            CustomButtonPrimary {
                id: btnAccept
                enabled: acceptable
                text: qsTr("CONFIRM")
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                onClicked: {
                    if (walletInfo.encType === NsWallet.Password) {
                        var passwordData = qmlFactory.createPasswordData()
                        passwordData.textPassword = walletDetailsFrame.password

                        if (fullBackupMode) {
                            if (walletsProxy.backupPrivateKey(walletInfo.walletId
                                                              , targetDir + "/" + backupFileName
                                                              , isPrintable
                                                              , passwordData)) {
                                var mb = JsHelper.messageBox(BSMessageBox.Type.Success
                                                             , qsTr("Wallet")
                                                             , qsTr("Wallet backup was successful.")
                                                             , qsTr("Wallet Name: %1\nWallet ID: %2\nBackup location: '%3'")
                                                             .arg(walletInfo.name)
                                                             .arg(walletInfo.walletId)
                                                             .arg(targetDir))

                                mb.bsAccepted.connect(function(){ acceptAnimated() })
                            }
                        }
                    }
                    else {
                        JsHelper.requesteIdAuth(AutheIDClient.BackupWallet
                                                , walletInfo
                                                , function(passwordData){
                                                    if (fullBackupMode) {
                                                        if (walletsProxy.backupPrivateKey(walletInfo.walletId
                                                                                          , targetDir + "/" + backupFileName
                                                                                          , isPrintable
                                                                                          , passwordData)) {
                                                            var mb = JsHelper.messageBox(BSMessageBox.Type.Success
                                                                                         , qsTr("Wallet")
                                                                                         , qsTr("Wallet backup was successful.")
                                                                                         , qsTr("Wallet Name: %1\nWallet ID: %2\nBackup location: '%3'")
                                                                                         .arg(walletInfo.name)
                                                                                         .arg(walletInfo.walletId)
                                                                                         .arg(targetDir))

                                                            mb.bsAccepted.connect(function(){ acceptAnimated() })
                                                        }

                                                    }
                                                }) // function(passwordData)
                    }
                }
            }
        }
    }
}
