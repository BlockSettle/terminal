import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2

import com.blocksettle.AuthSignWalletObject 1.0
import com.blocksettle.WalletInfo 1.0
import com.blocksettle.QSeed 1.0
import com.blocksettle.AutheIDClient 1.0
import com.blocksettle.QPasswordData 1.0
import com.blocksettle.NsWallet.namespace 1.0

import "../StyledControls"
import "../BsControls"
import "../js/helper.js" as JsHelper

CustomTitleDialogWindow {
    id: root

    property WalletInfo walletInfo: WalletInfo{}
    property string woWalletFileName
    property bool acceptable:(walletInfo.encType === NsWallet.Unencrypted)
                             || walletInfo.encType === NsWallet.Auth
                             || walletDetailsFrame.password.length
    property string exportDir: Qt.resolvedUrl(".")
    property AuthSignWalletObject authSign

    title: walletInfo ? qsTr("Export Watching-Only Copy of %1").arg(walletInfo.name) : ""

    width: 400
    height: 400
    rejectable: true
    onEnterPressed: {
        if (btnAccept.enabled) btnAccept.onClicked()
    }

    Component.onCompleted: {
        exportDir = decodeURIComponent(signerSettings.exportWalletsDir)
    }

    onWalletInfoChanged: {
        walletDetailsFrame.walletInfo = walletInfo
        woWalletFileName = walletsProxy.getWoWalletFile(walletInfo.walletId)
    }

    cContentItem: ColumnLayout {
        spacing: 10

        BSWalletDetailsFrame {
            id: walletDetailsFrame
            walletInfo: walletInfo
            inputsWidth: 250
            onPasswordEntered:{
                if (btnAccept.enabled) btnAccept.onClicked()
            }
        }

        CustomHeader {
            text: qsTr("Export Watching-Only Copy")
            Layout.fillWidth: true
            Layout.preferredHeight: 25
            Layout.topMargin: 5
            Layout.leftMargin: 10
            Layout.rightMargin: 10
        }

        RowLayout {
            width: parent.width
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomLabel {
                Layout.minimumWidth: 110
                Layout.preferredWidth: 110
                Layout.maximumWidth: 110
                text: qsTr("Export to file:")
                Layout.fillWidth: true
                verticalAlignment: Text.AlignTop
            }
            CustomLabelValue {
                text: qsTr("%1/%2").arg(exportDir).arg(woWalletFileName)
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }

        RowLayout {
            width: parent.width
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomButton {
                text: qsTr("Select Target Dir")
                Layout.minimumWidth: 80
                Layout.preferredWidth: 80
                Layout.maximumWidth: 80
                Layout.maximumHeight: 25
                Layout.leftMargin: 110 + 5
                onClicked: {
                    if (!ldrWoWalletDirDlg.item) {
                        ldrWoWalletDirDlg.active = true
                    }
                    ldrWoWalletDirDlg.startFromFolder = exportDir
                    ldrWoWalletDirDlg.item.accepted.connect(function() {
                        exportDir = ldrWoWalletDirDlg.dir
                    })
                    ldrWoWalletDirDlg.item.open();
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
            CustomButton {
                text: qsTr("Cancel")
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                onClicked: rejectAnimated()
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

                        walletsProxy.exportWatchingOnly(walletInfo.walletId, exportDir, passwordData)
                        var mb = JsHelper.messageBox(BSMessageBox.Type.Success
                                   , qsTr("Wallet")
                                   , qsTr("Watching-Only Wallet exported.")
                                   , qsTr("Wallet Name: %1\nWallet ID: %2\nBackup location: '%3'").arg(walletInfo.name).arg(walletInfo.walletId).arg(exportDir))

                        mb.accepted.connect(function(){ acceptAnimated() })
                    }
                    else {
                        JsHelper.requesteIdAuth(AutheIDClient.BackupWallet, walletInfo
                            , function(passwordData){
                                walletsProxy.exportWatchingOnly(walletInfo.walletId, exportDir, passwordData)
                                var mb = JsHelper.messageBox(BSMessageBox.Type.Success
                                           , qsTr("Wallet")
                                           , qsTr("Watching-Only wallet exported.")
                                           , qsTr("Wallet Name: %1\nWallet ID: %2\nBackup location: '%3'").arg(walletInfo.name).arg(walletInfo.walletId).arg(exportDir))

                                mb.accepted.connect(function(){ acceptAnimated() })
                            })
                    }
                }
            }
        }
    }
}
