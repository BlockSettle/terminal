import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Dialogs 1.2
import QtQuick.Layouts 1.3
import QtQml.Models 2.3

import com.blocksettle.WalletsViewModel 1.0
import com.blocksettle.WalletsProxy 1.0
import com.blocksettle.WalletInfo 1.0
import com.blocksettle.AuthSignWalletObject 1.0
import com.blocksettle.AutheIDClient 1.0
import com.blocksettle.QmlFactory 1.0
import com.blocksettle.QSeed 1.0
import com.blocksettle.QPasswordData 1.0

import "StyledControls"
import "BsStyles"
import "BsControls"
import "BsDialogs"
import "js/helper.js" as JsHelper

Item {
    id: view

    function isHdRoot() {
        var isRoot = walletsView.model.data(walletsView.selection.currentIndex, WalletsModel.IsHDRootRole)
        return ((typeof(isRoot) != "undefined") && isRoot)
    }
    function isAnyWallet() {
        var walletId = walletsView.model.data(walletsView.selection.currentIndex, WalletsModel.WalletIdRole)
        return ((typeof(walletId) != "undefined") && walletId.length)
    }

    Connections {
        target: walletsProxy
        onWalletError: {
            JsHelper.messageBoxCritical(qsTr("Error")
                                        , qsTr("Unable to complete this action.")
                                        , qsTr("%1").arg(errMsg))
        }
    }
    Connections {
        target: walletsView.model
        onModelReset: {
            // when model resetted selectionChanged signal is not emitted
            // button states needs to be updated after model reset, this emitted signal will do that
            var idx = walletsView.model.index(-1,-1);
            walletsView.selection.currentChanged(idx, idx)
        }
    }

    function getCurrentWalletInfo() {
        return qmlFactory.createWalletInfo(walletsView.model.data(walletsView.selection.currentIndex, WalletsModel.WalletIdRole))
    }

    ScrollView {
        id: scrollView
        anchors.fill: parent
        clip: true

        ColumnLayout {
            id: colWallets
            width: view.width
            spacing: 5

            CustomButtonBar {
                id: rowButtons
                implicitHeight: childrenRect.height
                Layout.fillWidth: true

                Flow {
                    id: buttonRow
                    spacing: 5
                    padding: 5
                    height: childrenRect.height + 10
                    width: parent.width

                    CustomButtonPrimary {
                        Layout.fillWidth: true
                        text: qsTr("New Wallet")
                        onClicked: {
                            // let user create a new wallet or import one from file
                            var dlgNew = Qt.createComponent("BsDialogs/WalletNewDialog.qml").createObject(mainWindow)
                            dlgNew.accepted.connect(function() {
                                if (dlgNew.type === WalletNewDialog.WalletType.NewWallet) {
                                    // TODO rename signerSettings.testNet -> signerSettings.isTestNet
                                    var newSeed = qmlFactory.createSeed(signerSettings.testNet)

                                    // allow user to save wallet seed lines and then prompt him to enter them for verification
                                    var dlgNewSeed = Qt.createComponent("BsDialogs/WalletNewSeedDialog.qml").createObject(mainWindow)
                                    dlgNewSeed.seed = newSeed
                                    dlgNewSeed.accepted.connect(function() {
                                        // let user set a password or Auth eID and also name and desc. for the new wallet
                                        var dlgCreateWallet = Qt.createComponent("BsDialogs/WalletCreateDialog.qml").createObject(mainWindow)
                                        dlgCreateWallet.primaryWalletExists = walletsProxy.primaryWalletExists

                                        dlgCreateWallet.seed = newSeed
                                        dlgCreateWallet.open();
                                    })
                                    dlgNewSeed.open()
                                }
                                else {
                                    var dlgImp = Qt.createComponent("BsDialogs/WalletImportDialog.qml").createObject(mainWindow)
                                    dlgImp.primaryWalletExists = walletsProxy.primaryWalletExists
                                    dlgImp.open()
                                }
                            })
                            dlgNew.open()
                        }
                    }

                    CustomButtonPrimary {
                        Layout.fillWidth: true
                        text: qsTr("Manage Encryption")
                        enabled: isHdRoot()
                        onClicked: {
                            var dlg = Qt.createComponent("BsDialogs/WalletChangePasswordDialog.qml").createObject(mainWindow)
                            dlg.walletInfo = getCurrentWalletInfo()
                            dlg.open()
                        }
                    }

                    CustomButtonPrimary {
                        Layout.fillWidth: true
                        enabled: isHdRoot()
                        text: qsTr("Delete Wallet")
                        onClicked: {
                            var walletId = walletsView.model.data(walletsView.selection.currentIndex, WalletsModel.WalletIdRole)
                            var walletName = walletsView.model.data(walletsView.selection.currentIndex, WalletsModel.NameRole)
                            var dlg = Qt.createComponent("BsDialogs/WalletDeleteDialog.qml").createObject(mainWindow)
                            dlg.walletId = walletId
                            dlg.walletName = walletName
                            dlg.isRootWallet = isHdRoot()
                            dlg.rootName = walletsProxy.getRootWalletName(dlg.walletId)
                            dlg.accepted.connect(function() {
                                if (dlg.backup) {
                                    var dlgBkp = Qt.createComponent("BsDialogs/WalletBackupDialog.qml").createObject(mainWindow)
                                    dlgBkp.walletInfo = getCurrentWalletInfo()
                                    dlgBkp.targetDir = signerSettings.dirDocuments
                                    dlgBkp.accepted.connect(function() {
                                        if (walletsProxy.deleteWallet(walletId)) {
                                            JsHelper.messageBox(BSMessageBox.Type.Success
                                                                , qsTr("Wallet")
                                                                , qsTr("Wallet successfully deleted.")
                                                                , qsTr("Wallet Name: %1\nWallet ID: %2").arg(walletName).arg(walletId))
                                        }
                                    })
                                    dlgBkp.open()
                                }
                                else {
                                    if (walletsProxy.deleteWallet(walletId)) {
                                        JsHelper.messageBox(BSMessageBox.Type.Success
                                                            , qsTr("Wallet")
                                                            , qsTr("Wallet successfully deleted.")
                                                            , qsTr("Wallet Name: %1\nWallet ID: %2").arg(dlg.walletName).arg(dlg.walletId))
                                    }
                                }
                            })
                            dlg.open()
                        }
                    }

                    CustomButtonPrimary {
                        Layout.fillWidth: true
                        text: qsTr("Backup Private Key")
                        enabled: isHdRoot()
                        onClicked: {
                            var dlg = Qt.createComponent("BsDialogs/WalletBackupDialog.qml").createObject(mainWindow)
                            dlg.walletInfo = getCurrentWalletInfo()
                            dlg.targetDir = signerSettings.dirDocuments
                            dlg.open()
                        }
                    }

                    CustomButtonPrimary {
                        Layout.fillWidth: true
                        text: qsTr("Export w/o Wallet")
                        enabled: isHdRoot()
                        onClicked: {
                            var dlg = Qt.createComponent("BsDialogs/WalletExportWoDialog.qml").createObject(mainWindow)
                            dlg.walletInfo = getCurrentWalletInfo()
                            dlg.open()
                        }
                    }
                }
            }

            CustomHeader {
                id: header
                text: qsTr("Wallet List")
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

            WalletsView {
                id: walletsView
                implicitWidth: view.width
                implicitHeight: view.height - rowButtons.height - header.height - colWallets.spacing * 3
            }
        }
    }
}
