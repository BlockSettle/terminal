import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Controls.Styles 1.4
import QtQuick.Dialogs 1.2
import QtQuick.Layouts 1.3
import com.blocksettle.Wallets 1.0
import com.blocksettle.WalletsProxy 1.0
import com.blocksettle.WalletInfo 1.0

Item {
    function isHdRoot() {
        var isRoot = walletsView.model.data(walletsView.currentIndex, WalletsModel.IsHDRootRole)
        return ((typeof(isRoot) != "undefined") && isRoot)
    }
    function isAnyWallet() {
        var walletId = walletsView.model.data(walletsView.currentIndex, WalletsModel.WalletIdRole)
        return ((typeof(walletId) != "undefined") && walletId.length)
    }

    Connections {
        target: walletsProxy
        onWalletError: {
            ibFailure.displayMessage(errMsg)
        }
    }

    id: view

    WalletInfo {
        id: walletInfo
    }

    function getCurrentWallet(view) {
        walletInfo.id = view.model.data(view.currentIndex, WalletsModel.WalletIdRole)
        walletInfo.rootId = view.model.data(view.currentIndex, WalletsModel.RootWalletIdRole)
        walletInfo.name = view.model.data(view.currentIndex, WalletsModel.NameRole)
        walletInfo.encKey = view.model.data(view.currentIndex, WalletsModel.EncKeyRole)
        walletInfo.encType = view.model.data(view.currentIndex, WalletsModel.IsEncryptedRole)
        return walletInfo
    }

    function exportWalletDialog(wallet) {
        var dlg = Qt.createQmlObject("WalletExportWoDialog {}", mainWindow, "exportWoDlg")
        dlg.wallet = wallet
        dlg.woWalletFileName = walletsProxy.getWoWalletFile(dlg.wallet.id)
        dlg.exportDir = decodeURIComponent(signerParams.walletsDir)
        console.log("exportWatchingOnly id:" + wallet.id + " name:" + wallet.name + " encType:" + wallet.encType + " encKey:" + wallet.encKey)
        dlg.accepted.connect(function() {
            console.log("exportWatchingOnly id:" + dlg.wallet.id + " dir:" + dlg.exportDir + " pwd:" + dlg.password)
            if (walletsProxy.exportWatchingOnly(dlg.wallet.id, dlg.exportDir, dlg.password)) {
                ibSuccess.displayMessage(qsTr("Successfully exported watching-only copy for wallet %1 (id %2) to %3")
                                         .arg(dlg.wallet.name).arg(dlg.wallet.id).arg(dlg.exportDir))
            }
        })
        dlg.open()
    }

    ScrollView {
        id: scrollView
        anchors.fill: parent
        clip:   true

        ColumnLayout {
            id:     colWallets
            width: view.width
            spacing: 5

            CustomHeader {
                id: header
                text:   qsTr("Wallet List")
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

            WalletsView {
                id: walletsView
                implicitWidth: view.width
                implicitHeight: view.height - rowButtons.height - header.height - colWallets.spacing * 3
            }

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

                    CustomButton {
                        Layout.fillWidth: true
                        text:   qsTr("New Wallet")
                        onClicked: {

                            //var walletInfo2 = getWalletById(walletsView, qsTr("28xFzXjFU"))
                            //var walletInfo2 = getCurrentWallet(walletsView)
                            //console.log("id:" + walletInfo2.id + " name:" + walletInfo2.name + " rootId:" + walletInfo2.rootId + " encType:" + walletInfo2.encType + " encKey:" + walletInfo2.encKey)

                            // let user create a new wallet or import one from file
                            var dlgNew = Qt.createQmlObject("WalletNewDialog {}", mainWindow, "WalletNewDialog")
                            dlgNew.accepted.connect(function() {
                                if (dlgNew.type === WalletNewDialog.WalletType.RandomSeed) {
                                    newWalletSeed.generate();
                                    // allow user to save wallet seed lines and then prompt him to enter them for verification
                                    var dlgSeed = Qt.createQmlObject("NewWalletSeedDialog {}", mainWindow, "NewWalletSeedDialog")
                                    dlgSeed.accepted.connect(function() {
                                        // let user set a password or Auth eID and also name and desc. for the new wallet
                                        var dlgPwd = Qt.createQmlObject("WalletCreateDialog {}", mainWindow, "walletCreateDlg")
                                        dlgPwd.primaryWalletExists = walletsProxy.primaryWalletExists
                                        dlgPwd.seed = walletsProxy.createWalletSeed()
                                        if (!dlgPwd.seed.parsePaperKey(newWalletSeed.part1 + "\n" + newWalletSeed.part2)) {
                                            ibFailure.displayMessage("Failed to parse wallet seed")
                                        }
                                        else {
                                            dlgPwd.open();
                                        }

                                        dlgPwd.accepted.connect(function(){
                                            // create the wallet

                                            if (walletsProxy.createWallet(dlgPwd.isPrimary, dlgPwd.password, dlgPwd.seed)) {
                                                ibSuccess.displayMessage(qsTr("New wallet <%1> successfully created").arg(dlgPwd.seed.walletName))
                                                // open export dialog to give user a chance to export the wallet
                                                walletInfo.id = dlgPwd.seed.walletId
                                                walletInfo.rootId = dlgPwd.seed.walletId
                                                walletInfo.name = dlgPwd.seed.walletName
                                                walletInfo.encType = dlgPwd.encType
                                                walletInfo.encKey = dlgPwd.encKey
                                                console.log("createWallet id:" + dlgPwd.seed.walletId + " name:" + dlgPwd.seed.walletName + " encType:" + dlgPwd.encType)
                                                exportWalletDialog(walletInfo)
                                            }
                                        })

                                    })
                                    dlgSeed.open()
                                }
                                else {
                                    var dlgImp = Qt.createQmlObject("WalletImportDialog {}", mainWindow, "walletImportDlg")
                                    dlgImp.primaryWalletExists = walletsProxy.primaryWalletExists
                                    dlgImp.digitalBackup = (dlgNew.type === WalletNewDialog.WalletType.DigitalBackupFile)
                                    dlgImp.seed = walletsProxy.createWalletSeed()
                                    dlgImp.accepted.connect(function(){
                                        if (walletsProxy.importWallet(dlgImp.isPrimary, dlgImp.seed, dlgImp.password)) {
                                            ibSuccess.displayMessage(qsTr("Successfully imported wallet <%1>")
                                                                     .arg(dlgImp.seed.walletName))
                                        }
                                    })
                                    dlgImp.open()
                                }
                            })
                            dlgNew.open()
                        }
                    }

                    CustomButton {
                        Layout.fillWidth: true
                        text:   qsTr("Change Password")
                        enabled:    isHdRoot()
                        onClicked: {
                            var dlg = Qt.createQmlObject("WalletChangePasswordDialog {}", mainWindow, "changePwDlg")
                            dlg.wallet = getCurrentWallet(walletsView)
                            dlg.accepted.connect(function() {
                                if (walletsProxy.changePassword(dlg.walletId, dlg.oldPassword, dlg.newPassword,
                                                                dlg.wallet.encType, dlg.wallet.encKey)) {
                                    ibSuccess.displayMessage(qsTr("New password successfully set for %1").arg(dlg.walletName))
                                }
                            })
                            dlg.open()
                        }
                    }

                    CustomButton {
                        Layout.fillWidth: true
                        enabled:    isAnyWallet()
                        text:   qsTr("Delete Wallet")
                        onClicked: {
                            var dlg = Qt.createQmlObject("WalletDeleteDialog {}", mainWindow, "walletDeleteDlg")
                            dlg.walletId = walletsView.model.data(walletsView.currentIndex, WalletsModel.WalletIdRole)
                            dlg.walletName = walletsView.model.data(walletsView.currentIndex, WalletsModel.NameRole)
                            dlg.isRootWallet = isHdRoot()
                            dlg.rootName = walletsProxy.getRootWalletName(dlg.walletId)
                            dlg.accepted.connect(function() {
                                if (dlg.backup) {
                                    var dlgBkp = Qt.createQmlObject("WalletBackupDialog {}", mainWindow, "walletBackupDlg")
                                    dlgBkp.wallet = getCurrentWallet(walletsView)
                                    dlgBkp.targetDir = signerParams.dirDocuments
                                    dlgBkp.accepted.connect(function() {
                                        if (walletsProxy.backupPrivateKey(dlgBkp.wallet.id, dlgBkp.targetDir + "/" + dlgBkp.backupFileName
                                                                          , dlgBkp.isPrintable, dlgBkp.password)) {
                                            if (walletsProxy.deleteWallet(dlg.walletId)) {
                                                ibSuccess.displayMessage(qsTr("Wallet <%1> (id %2) was deleted")
                                                                         .arg(dlg.walletName).arg(dlg.walletId))
                                            }
                                        }
                                    })
                                    dlgBkp.open()
                                }
                                else {
                                    if (walletsProxy.deleteWallet(dlg.walletId)) {
                                        ibSuccess.displayMessage(qsTr("Wallet <%1> (id %2) was deleted")
                                                                 .arg(dlg.walletName).arg(dlg.walletId))
                                    }
                                }
                            })
                            dlg.open()
                        }
                    }

                    CustomButton {
                        Layout.fillWidth: true
                        text:   qsTr("Backup Private Key")
                        enabled:    isHdRoot()
                        onClicked: {
                            var dlg = Qt.createQmlObject("WalletBackupDialog {}", mainWindow, "walletBackupDlg")
                            dlg.wallet = getCurrentWallet(walletsView)
                            dlg.targetDir = signerParams.dirDocuments
                            dlg.accepted.connect(function() {
                                if (walletsProxy.backupPrivateKey(dlg.wallet.id, dlg.targetDir + "/" + dlg.backupFileName
                                                                  , dlg.isPrintable, dlg.password)) {
                                    ibSuccess.displayMessage(qsTr("Backup of wallet %1 (id %2) to %3/%4 was successful")
                                                             .arg(dlg.wallet.name).arg(dlg.wallet.id)
                                                             .arg(dlg.targetDir).arg(dlg.backupFileName))
                                }
                            })
                            dlg.open()
                        }
                    }

                    CustomButton {
                        Layout.fillWidth: true
                        text:   qsTr("Export Watching Only Wallet")
                        enabled:    isHdRoot()
                        onClicked: {
                            exportWalletDialog(getCurrentWallet(walletsView))
                        }
                    }
                }
            }
        }
    }
}
