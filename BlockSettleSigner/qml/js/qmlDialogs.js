.import "helper.js" as JsHelper

function customDialogRequest(dialogName, data) {
    //if (dialogName === "createNewWalletDialog")
    let dlg = eval(dialogName)(data)
    JsHelper.raiseWindow()
    return dlg
}

function evalWorker(method, cppCallback, argList) {
    let jsCallback = function(cbArg0, cbArg1, cbArg2, cbArg3, cbArg4, cbArg5, cbArg6, cbArg7){
        let cbArgList = new Array(7)

        if (typeof cbArg0 !== 'undefined') cbArgList[0] = cbArg0
        if (typeof cbArg1 !== 'undefined') cbArgList[1] = cbArg1
        if (typeof cbArg2 !== 'undefined') cbArgList[2] = cbArg2
        if (typeof cbArg3 !== 'undefined') cbArgList[3] = cbArg3
        if (typeof cbArg4 !== 'undefined') cbArgList[4] = cbArg4
        if (typeof cbArg5 !== 'undefined') cbArgList[5] = cbArg5
        if (typeof cbArg6 !== 'undefined') cbArgList[6] = cbArg6
        if (typeof cbArg7 !== 'undefined') cbArgList[7] = cbArg7

        cppCallback.exec(cbArgList)
    }

    let val0 = argList[0];
    let val1 = argList[1];
    let val2 = argList[2];
    let val3 = argList[3];
    let val4 = argList[4];
    let val5 = argList[5];
    let val6 = argList[6];
    let val7 = argList[7];

         if (typeof val7 !== 'undefined') eval(method)(jsCallback, val0, val1, val2, val3, val4, val5, val6, val7)
    else if (typeof val6 !== 'undefined') eval(method)(jsCallback, val0, val1, val2, val3, val4, val5, val6)
    else if (typeof val5 !== 'undefined') eval(method)(jsCallback, val0, val1, val2, val3, val4, val5)
    else if (typeof val4 !== 'undefined') eval(method)(jsCallback, val0, val1, val2, val3, val4)
    else if (typeof val3 !== 'undefined') eval(method)(jsCallback, val0, val1, val2, val3)
    else if (typeof val2 !== 'undefined') eval(method)(jsCallback, val0, val1, val2)
    else if (typeof val1 !== 'undefined') eval(method)(jsCallback, val0, val1)
    else if (typeof val0 !== 'undefined') eval(method)(jsCallback, val0)
    else if (typeof cppCallback !== 'undefined') eval(method)(jsCallback)
    else                                  eval(method)()
}

function prepareLigthModeDialog(dialog) {
    // close previous dialog
    if (currentDialog && typeof currentDialog.close !== "undefined") {
        currentDialog.close()
    }

    show()
    currentDialog = dialog

    mainWindow.width = currentDialog.width
    mainWindow.height = currentDialog.height
    mainWindow.moveMainWindowToScreenCenter()
    mainWindow.title = currentDialog.title
    if (typeof currentDialog.qmlTitleVisible !== "undefined") {
        currentDialog.qmlTitleVisible = false
    }

    currentDialog.dialogsChainFinished.connect(function(){ hide() })
    currentDialog.nextChainDialogChangedOverloaded.connect(function(nextDialog){
        mainWindow.width = nextDialog.width
        mainWindow.height = nextDialog.height
        mainWindow.moveMainWindowToScreenCenter()

        nextDialog.sizeChanged.connect(function(w, h){
            mainWindow.width = w
            mainWindow.height = h
            mainWindow.moveMainWindowToScreenCenter()
        })
    })

    currentDialog.sizeChanged.connect(function(w, h){
        mainWindow.width = w
        mainWindow.height = h
        mainWindow.moveMainWindowToScreenCenter()
    })
}


function createNewWalletDialog(data) {
    var newSeed = qmlFactory.createSeed(signerSettings.testNet)

    // allow user to save wallet seed lines and then prompt him to enter them for verification
    var dlgNewSeed = Qt.createComponent("../BsDialogs/WalletNewSeedDialog.qml").createObject(mainWindow)
    dlgNewSeed.seed = newSeed
    dlgNewSeed.bsAccepted.connect(function() {
        // let user set a password or Auth eID and also name and desc. for the new wallet
        var dlgCreateWallet = Qt.createComponent("../BsDialogs/WalletCreateDialog.qml").createObject(mainWindow)
        dlgNewSeed.setNextChainDialog(dlgCreateWallet)
        dlgCreateWallet.seed = newSeed
        dlgCreateWallet.bsResized.connect(function() {
            mainWindow.moveMainWindowToScreenCenter()
        })
        dlgCreateWallet.open()
    })
    if (Object.keys(mainWindow).indexOf("currentDialog") != -1) {
        mainWindow.sizeChanged.connect(function(w, h) {
            dlgNewSeed.width = w
            dlgNewSeed.height = h
        })
    }
    dlgNewSeed.open()
    return dlgNewSeed
}

function importWalletDialog(data) {
    var dlgImp = Qt.createComponent("../BsDialogs/WalletImportDialog.qml").createObject(mainWindow)
    dlgImp.open()
    return dlgImp
}

function backupWalletDialog(data) {
    var rootId = data["rootId"]
    var dlg = Qt.createComponent("../BsDialogs/WalletBackupDialog.qml").createObject(mainWindow)
    dlg.walletInfo = qmlFactory.createWalletInfo(rootId)
    // FIXME: save backups dir
    //dlg.targetDir = signerSettings.dirDocuments
    dlg.open()
    return dlg
}

function deleteWalletDialog(data) {
    var walletId = data["rootId"]
    var dlg = Qt.createComponent("../BsDialogs/WalletDeleteDialog.qml").createObject(mainWindow)
    dlg.walletInfo = qmlFactory.createWalletInfo(walletId)
    dlg.rootName = walletsProxy.getRootWalletName(walletId)
    dlg.bsResized.connect(function() {
        mainWindow.moveMainWindowToScreenCenter()
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

function activateAutoSignDialog(data) {
    var walletId = data["rootId"]
    signerSettings.autoSignWallet = walletId
    signerStatus.activateAutoSign()
}

function createCCSettlementTransactionDialog(jsCallback, prompt, txInfo, settlementInfo, walletInfo) {
    raiseWindow()

    var dlg = Qt.createComponent("../BsDialogs/CCSettlementTransactionDialog.qml").createObject(mainWindow)
    prepareLigthModeDialog(dlg)

    dlg.walletInfo = walletInfo
    dlg.prompt = prompt
    dlg.txInfo = txInfo
    dlg.settlementInfo = settlementInfo

    // FIXME: use bs error codes enum in qml
    dlg.bsAccepted.connect(function() {
        jsCallback(0, walletInfo.walletId, dlg.passwordData)
    })
    dlg.bsRejected.connect(function() {
        jsCallback(10, walletInfo.walletId, dlg.passwordData)
    })
    mainWindow.requestActivate()
    dlg.open()

    dlg.init()
}
