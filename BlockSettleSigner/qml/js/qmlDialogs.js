.import "helper.js" as JsHelper

function customDialogRequest(dialogName, data) {
    //if (dialogName === "createNewWalletDialog")
    var dlg = eval(dialogName)(data)
    JsHelper.raiseWindow()
    return dlg
}

function createNewWalletDialog(data) {
    var newSeed = qmlFactory.createSeed(signerSettings.testNet)

    // allow user to save wallet seed lines and then prompt him to enter them for verification
    var dlgNewSeed = Qt.createComponent("../BsDialogs/WalletNewSeedDialog.qml").createObject(mainWindow)
    dlgNewSeed.seed = newSeed
    dlgNewSeed.bsAccepted.connect(function() {
        // let user set a password or Auth eID and also name and desc. for the new wallet
        var dlgCreateWallet = Qt.createComponent("../BsDialogs/WalletCreateDialog.qml").createObject(mainWindow)
        dlgCreateWallet.primaryWalletExists = walletsProxy.primaryWalletExists

        dlgCreateWallet.seed = newSeed
        dlgCreateWallet.open()
    })
    dlgNewSeed.open()
    return dlgNewSeed
}

function importWalletDialog(data) {
    var dlgImp = Qt.createComponent("../BsDialogs/WalletImportDialog.qml").createObject(mainWindow)
    dlgImp.primaryWalletExists = walletsProxy.primaryWalletExists
    dlgImp.open()
    return dlgImp
}

function backupWalletDialog(data) {
    var rootId = data["rootId"]
    var dlg = Qt.createComponent("../BsDialogs/WalletBackupDialog.qml").createObject(mainWindow)
    dlg.walletInfo = qmlFactory.createWalletInfo(rootId)
    dlg.targetDir = signerSettings.dirDocuments
    dlg.open()
    return dlg
}

function deleteWalletDialog(data) {
    var walletId = data["rootId"]
    var dlg = Qt.createComponent("../BsDialogs/WalletDeleteDialog.qml").createObject(mainWindow)
    dlg.walletInfo = qmlFactory.createWalletInfo(walletId)
    dlg.rootName = walletsProxy.getRootWalletName(walletId)

    dlg.bsAccepted.connect(function() {
        if (dlg.backup) {
            var dlgBkp = Qt.createComponent("../BsDialogs/WalletBackupDialog.qml").createObject(mainWindow)
            dlg.setNextChainDialog(dlgBkp)
            dlgBkp.walletInfo = qmlFactory.createWalletInfo(walletId)
            dlgBkp.targetDir = signerSettings.dirDocuments
            dlgBkp.bsAccepted.connect(function() {
                if (walletsProxy.deleteWallet(walletId)) {
                    JsHelper.messageBox(BSMessageBox.Type.Success
                                        , qsTr("Wallet")
                                        , qsTr("Wallet successfully deleted.")
                                        , qsTr("Wallet Name: %1\nWallet ID: %2").arg(dlg.walletInfo.name).arg(walletId))
                }
            })
            dlgBkp.open()
        }
        else {
            if (walletsProxy.deleteWallet(walletId)) {
                JsHelper.messageBox(BSMessageBox.Type.Success
                                    , qsTr("Wallet")
                                    , qsTr("Wallet successfully deleted.")
                                    , qsTr("Wallet Name: %1\nWallet ID: %2").arg(dlg.walletInfo.name).arg(walletId))
            }
        }
    })
    dlg.open()
    return dlg
}

function manageEncryptionDialog(data) {
    var rootId = data["rootId"]
    var dlg = Qt.createComponent("../BsDialogs/WalletManageEncryptionDialog.qml").createObject(mainWindow)
    dlg.walletInfo = qmlFactory.createWalletInfo(rootId)
    dlg.open()
    return dlg
}
