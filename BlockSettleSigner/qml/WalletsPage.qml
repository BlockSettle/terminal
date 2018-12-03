import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Controls.Styles 1.4
import QtQuick.Dialogs 1.2
import QtQuick.Layouts 1.3
import com.blocksettle.Wallets 1.0
import com.blocksettle.WalletsProxy 1.0
import com.blocksettle.WalletInfo 1.0

Item {
    id: view
    property string autheIdTitle: qsTr("Signing with Auth eID")
    property string autheIdNotice: qsTr("Auth eID is a convenient alternative to passwords. Instead of entering a password, BlockSettle Terminal issues a secure notification to mobile devices attached to your wallet's Auth eID account. You may then sign wallet-related requests via a press of a button in the Auth eID app on your mobile device(s).<br><br>You may add or remove devices to your Auth eID accounts as required by the user, and users may have multiple devices on one account. Auth eID requires the user to be vigilant with devices using Auth eID. If a device is damaged or lost, the user will be unable to sign Auth eID requests, and the wallet will become unusable.<br><br>Auth eID is not a wallet backup! No wallet data is stored with Auth eID. Therefore, you must maintain proper backups of your wallet's Root Private Key (RPK). In the event that all mobile devices attached to a wallet are damaged or lost, the RPK may be used to create a duplicate wallet. You may then attach a password or your Auth eID account to the wallet.<br><br>Auth eID, like any software, is susceptible to malware, although keyloggers will serve no purpose. Please keep your mobile devices up-to-date with the latest software updates, and never install software offered outside your device's app store.<br><br>For more information, please consult:<br><a href=\"http://pubb.blocksettle.com/PDF/AutheID%20Getting%20Started.pdf\"><span style=\"color:white;\">Getting Started With Auth eID</span></a> guide.")
    property string passwordConfirmTitle: qsTr("Please take care of your assets!")
    property string passwordConfirmNotice: qsTr("No one can help you recover your bitcoins if you forget the passphrase and don't have a backup! Your Wallet and any backups are useless if you lose them.<br><br>A backup protects your wallet forever, against hard drive loss and losing your passphrase. It also protects you from theft, if the wallet was encrypted and the backup wasn't stolen with it. Please make a backup and keep it in a safe place.<br><br>Please enter your passphrase one more time to indicate that you are aware of the risks of losing your passphrase!")

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
            messageBoxCritical(qsTr("Error"), qsTr("Unable to complete this action."), qsTr("%1").arg(errMsg))
        }
    }

    WalletInfo {
        id: walletInfo
    }

    BSMessageBox {
        id: abortBox
        type: BSMessageBox.Type.Question
        titleText: qsTr("Warning")
        text: bWalletCreate ? qsTr("Abort Wallet Creation?") : qsTr("Do you want to abort Wallet Import?")
        details: bWalletCreate ? qsTr("The Wallet will not be created if you don't complete the procedure.\n\nAre you sure you want to abort the Wallet Creation process?")
                               : qsTr("The Wallet will not be imported if you don't complete the procedure.\n\nAre you sure you want to abort the Wallet Import process?")
        acceptButtonText: bWalletCreate ? qsTr("Abort") : qsTr("Abort\nWallet Import")
        property bool bWalletCreate: true
    }

    // used for display password confirm and auth eid notice
    BSMessageBox {
        id: noticeBox
        type: BSMessageBox.Type.Info
        titleText: qsTr("Notice!")
        text: passwordNotice ? passwordConfirmTitle : autheIdTitle
        details: passwordNotice ? passwordConfirmNotice : autheIdNotice
        usePassword: passwordNotice
        rejectButtonVisible: passwordNotice
        property bool passwordNotice: false
        textFormat: Text.RichText
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
                messageBox(BSMessageBox.Type.Success, qsTr("Wallet"), qsTr("Successfully exported watching-only copy for wallet."),
                           qsTr("Wallet Name: %1\nWallet ID: %2\nBackup location: '%3'").arg(dlg.wallet.name).arg(dlg.wallet.id).arg(dlg.exportDir))
                //ibSuccess.displayMessage(qsTr("Successfully exported watching-only copy for wallet %1 (id %2) to %3")
                //                         .arg(dlg.wallet.name).arg(dlg.wallet.id).arg(dlg.exportDir))
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
                            // let user create a new wallet or import one from file
                            var dlgNew = Qt.createQmlObject("WalletNewDialog {}", mainWindow, "WalletNewDialog")
                            dlgNew.accepted.connect(function() {
                                if (dlgNew.type === WalletNewDialog.WalletType.NewWallet) {
                                    newWalletSeed.generate();
                                    // allow user to save wallet seed lines and then prompt him to enter them for verification
                                    var dlgSeed = Qt.createQmlObject("NewWalletSeedDialog {}", mainWindow, "NewWalletSeedDialog")
                                    dlgSeed.accepted.connect(function() {
                                        // let user set a password or Auth eID and also name and desc. for the new wallet
                                        var dlgPwd = Qt.createQmlObject("WalletCreateDialog {}", mainWindow, "walletCreateDlg")
                                        dlgPwd.primaryWalletExists = walletsProxy.primaryWalletExists
                                        dlgPwd.seed = walletsProxy.createWalletSeed()
                                        if (!dlgPwd.seed.parsePaperKey(newWalletSeed.part1 + "\n" + newWalletSeed.part2)) {
                                            messageBox(BSMessageBox.Type.Critical, qsTr("Error"), qsTr("Failed to parse wallet seed."), qsTr(""))
                                            //ibFailure.displayMessage("Failed to parse wallet seed")
                                        }
                                        else {
                                            dlgPwd.open();
                                        }

                                        dlgPwd.accepted.connect(function(){
                                            // create the wallet

                                            if (walletsProxy.createWallet(dlgPwd.isPrimary, dlgPwd.password, dlgPwd.seed)) {
                                                // open export dialog to give user a chance to export the wallet
                                                walletInfo.id = dlgPwd.seed.walletId
                                                walletInfo.rootId = dlgPwd.seed.walletId
                                                walletInfo.name = dlgPwd.seed.walletName
                                                walletInfo.encType = dlgPwd.encType
                                                walletInfo.encKey = dlgPwd.encKey
                                                exportWalletDialog(walletInfo)
                                                messageBox(BSMessageBox.Type.Success, qsTr("Wallet"), qsTr("Wallet successfully created."),
                                                           qsTr("Wallet ID: %1").arg(dlgPwd.seed.walletName))
                                            }
                                        })

                                    })
                                    dlgSeed.open()
                                }
                                else {
                                    var dlgImp = Qt.createQmlObject("WalletImportDialog {}", mainWindow, "walletImportDlg")
                                    dlgImp.primaryWalletExists = walletsProxy.primaryWalletExists
                                    dlgImp.seed = walletsProxy.createWalletSeed()
                                    dlgImp.accepted.connect(function(){
                                        if (walletsProxy.importWallet(dlgImp.isPrimary, dlgImp.seed, dlgImp.password)) {
                                            walletInfo.id = dlgImp.seed.walletId
                                            walletInfo.rootId = dlgImp.seed.walletId
                                            walletInfo.name = dlgImp.seed.walletName
                                            walletInfo.encType = dlgImp.encType
                                            walletInfo.encKey = dlgImp.encKey
                                            exportWalletDialog(walletInfo)
                                            messageBox(BSMessageBox.Type.Success, qsTr("Wallet"), qsTr("Wallet successfully imported."),
                                                       qsTr("Wallet ID: %1").arg(dlgImp.seed.walletName))
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
                                    messageBox(BSMessageBox.Type.Success, qsTr("Password"), qsTr("New password successfully set."), qsTr("Wallet ID: %1").arg(dlg.walletName))
                                    //ibSuccess.displayMessage(qsTr("New password successfully set for %1").arg(dlg.walletName))
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
                                                messageBox(BSMessageBox.Type.Success, qsTr("Wallet"), qsTr("Wallet was successfully deleted."),
                                                           qsTr("Wallet Name: %1\nWallet ID: %2").arg(dlg.walletName).arg(dlg.walletId))
                                                //ibSuccess.displayMessage(qsTr("Wallet <%1> (id %2) was deleted")
                                                //                         .arg(dlg.walletName).arg(dlg.walletId))
                                            }
                                        }
                                    })
                                    dlgBkp.open()
                                }
                                else {
                                    if (walletsProxy.deleteWallet(dlg.walletId)) {
                                        messageBox(BSMessageBox.Type.Success, qsTr("Wallet"), qsTr("Wallet was successfully deleted."),
                                                   qsTr("Wallet Name: %1\nWallet ID: %2").arg(dlg.walletName).arg(dlg.walletId))
                                        //ibSuccess.displayMessage(qsTr("Wallet <%1> (id %2) was deleted")
                                        //                        .arg(dlg.walletName).arg(dlg.walletId))
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
                                    messageBox(BSMessageBox.Type.Success, qsTr("Wallet"), qsTr("Wallet backup was successful."),
                                               qsTr("Wallet Name: %1\nWallet ID: %2\nBackup location: '%3'").arg(dlg.wallet.name).arg(dlg.wallet.id).arg(dlg.targetDir))
                                    //ibSuccess.displayMessage(qsTr("Backup of wallet %1 (id %2) to %3/%4 was successful")
                                    //                         .arg(dlg.wallet.name).arg(dlg.wallet.id).arg(dlg.targetDir).arg(dlg.backupFileName))
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
