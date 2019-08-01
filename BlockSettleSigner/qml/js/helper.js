// https://doc.qt.io/qt-5/qtqml-javascript-resources.html
// Don't use .import here
// Don't use .pragma library here


function openAbortBox(dialog, abortBoxType) {
    var abortBox = Qt.createComponent("../BsControls/BSAbortBox.qml").createObject(mainWindow)
    abortBox.abortType = abortBoxType
    abortBox.bsAccepted.connect(function() {dialog.rejectAnimated()} )
    abortBox.open()
}

function messageBox(type, title, text, details, parent) {
    var messageBox_ = Qt.createComponent("../BsControls/BSMessageBox.qml").createObject(mainWindow)

    messageBox_.type = type
    messageBox_.title = title
    messageBox_.customText = text
    if (details !== undefined) messageBox_.customDetails = details
    messageBox_.parent = parent
    if (type !== BSMessageBox.Type.Question) messageBox_.cancelButtonVisible = false
    messageBox_.open()

    return messageBox_
}

function resultBox(type, result, walletInfo) {
    var messageBox_ = Qt.createComponent("../BsControls/BSResultBox.qml").createObject(mainWindow)

    messageBox_.walletInfo = walletInfo
    messageBox_.resultType = type
    messageBox_.result_ = result
    messageBox_.open()

    return messageBox_
}


function messageBoxInfo(title, text, details, parent) {
    messageBox(BSMessageBox.Type.Info, title, text, details, parent);
}

function messageBoxCritical(title, text, details) {
    messageBox(BSMessageBox.Type.Critical, title, text, details);
}

function raiseWindow(w) {
    w.show()
    w.raise()
    w.requestActivate()
    w.flags |= Qt.WindowStaysOnTopHint
    w.flags &= ~Qt.WindowStaysOnTopHint
}

function hideWindow(w) {
    w.hide()
}

function requesteIdAuth (requestType, walletInfo, onSuccess) {
    var authObject = qmlFactory.createAutheIDSignObject(requestType, walletInfo)
    var authProgress = Qt.createComponent("../BsControls/BSEidProgressBox.qml").createObject(mainWindow);

    authProgress.email = walletInfo.email()
    authProgress.walletId = walletInfo.rootId
    authProgress.walletName = walletInfo.name
    authProgress.requestType = requestType

    authProgress.open()
    authProgress.bsRejected.connect(function() {
        authObject.destroy()
    })

    authObject.succeeded.connect(function(encKey_, password_) {
        authProgress.acceptAnimated()

        var passwordData = qmlFactory.createPasswordData()
        passwordData.encType = QPasswordData.Auth
        passwordData.encKey = encKey_
        passwordData.binaryPassword = password_

        onSuccess(passwordData);
        authObject.destroy()
    })
    authObject.failed.connect(function(errorText) {       
        console.log("QML requesteIdAuth: authObject.failed")
        var mb = messageBox(BSMessageBox.Type.Critical
                                     , qsTr("Wallet")
                                     , qsTr("eID request failed with error: \n") + errorText
                                     , qsTr("Wallet Name: %1\nWallet ID: %2")
                                     .arg(walletInfo.name)
                                     .arg(walletInfo.rootId))

        authProgress.setNextChainDialog(mb)
        authProgress.rejectAnimated()
        authObject.destroy()
    })
    authObject.userCancelled.connect(function() {
        console.log("QML requesteIdAuth: authObject.userCancelled")
        authProgress.rejectAnimated()
        authObject.destroy()
    })

    return authProgress
}

function removeEidDevice (index, walletInfo, onSuccess) {
    var authObject = qmlFactory.createRemoveEidObject(index, walletInfo)
    var authProgress = Qt.createComponent("../BsControls/BSEidProgressBox.qml").createObject(mainWindow);

    authProgress.email = walletInfo.email()
    authProgress.walletId = walletInfo.rootId
    authProgress.walletName = walletInfo.name
    authProgress.requestType = AutheIDClient.DeactivateWallet

    authProgress.open()
    authProgress.bsRejected.connect(function() {
        authObject.destroy()
    })

    authObject.succeeded.connect(function(encKey_, password_) {
        authProgress.acceptAnimated()

        var passwordData = qmlFactory.createPasswordData()
        passwordData.encType = QPasswordData.Auth
        passwordData.encKey = encKey_
        passwordData.binaryPassword = password_

        onSuccess(passwordData);
        authObject.destroy()
    })
    authObject.failed.connect(function(errorText) {
        messageBox(BSMessageBox.Type.Critical
                                     , qsTr("Wallet")
                                     , qsTr("eID request failed with error: \n") + errorText
                                     , qsTr("Wallet Name: %1\nWallet ID: %2")
                                     .arg(walletInfo.name)
                                     .arg(walletInfo.rootId))

        authProgress.rejectAnimated()
        authObject.destroy()
    })
}


function activateeIdAuth (email, walletInfo, onSuccess) {
    var authObject = qmlFactory.createActivateEidObject(email, walletInfo)
    var authProgress = Qt.createComponent("../BsControls/BSEidProgressBox.qml").createObject(mainWindow);

    authProgress.email = walletInfo.email()
    authProgress.walletId = walletInfo.rootId
    authProgress.walletName = walletInfo.name
    authProgress.requestType = AutheIDClient.ActivateWallet

    authProgress.open()
    authProgress.bsRejected.connect(function() {
        if (authObject !== undefined) authObject.destroy()
    })

    authObject.succeeded.connect(function(encKey_, password_) {
        authProgress.acceptAnimated()

        var passwordData = qmlFactory.createPasswordData()
        passwordData.encType = QPasswordData.Auth
        passwordData.encKey = encKey_
        passwordData.binaryPassword = password_

        onSuccess(passwordData);
        authObject.destroy()
    })
    authObject.failed.connect(function(errorText) {
        messageBox(BSMessageBox.Type.Critical
                                     , qsTr("Wallet")
                                     , qsTr("eID request failed with error: \n") + errorText
                                     , qsTr("Wallet Name: %1\nWallet ID: %2")
                                     .arg(walletInfo.name)
                                     .arg(walletInfo.rootId))

        authProgress.rejectAnimated()
        authObject.destroy()
    })
}

function parseEncKeyToEmail(encKey) {
    return encKey.split(':')[0]
}
function parseEncKeyToDeviceName(encKey) {
    return encKey.split(':')[2]
}

function fileUrlToPath(fileUrl) {
    var tmp = fileUrl.toString()
    tmp = tmp.replace(/(^file:\/{3})/, "")
    tmp = decodeURIComponent(tmp)
    return tmp
}
function folderOfFile(fileUrl) {
    var tmp = fileUrl.toString()
    tmp = tmp.replace(/(^file:\/{3})/, "")
    tmp = decodeURIComponent(tmp)
    tmp =  tmp.slice(0, tmp.lastIndexOf("/"))
    return tmp;
}

function openTextFile(fileUrl) {
    var request = new XMLHttpRequest();
    request.open("GET", fileUrl, false);
    request.send(null);
    return request.responseText;
}

function saveTextFile(fileUrl, text) {
    var request = new XMLHttpRequest();
    request.open("PUT", fileUrl, false);
    request.send(text);
    return request.status;
}

function isSelectedWalletHdRoot(walletsView) {
    var isRoot = walletsView.model.data(walletsView.selection.currentIndex, WalletsModel.IsHDRootRole)
    return ((typeof(isRoot) != "undefined") && isRoot)
}
//function isAnyWallet(walletsView) {
//    var walletId = walletsView.model.data(walletsView.selection.currentIndex, WalletsModel.WalletIdRole)
//    return ((typeof(walletId) != "undefined") && walletId.length)
//}

function customDialogRequest(dialogName, data) {
    // TODO: send initial values (qmlTitleVisible) in createObject params map
    let dlg = eval(dialogName)(data)
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

    // TODO: try to refactor using .apply
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

function prepareLiteModeDialog(dialog) {
    if (!isLiteMode()) {
        return
    }

    // close previous dialog
    if (currentDialog && typeof currentDialog.close !== "undefined") {
        currentDialog.close()
    }

    //dialog.show()
    currentDialog = dialog
//    if (typeof dialog.qmlTitleVisible !== "undefined") {
//        dialog.qmlTitleVisible = false
//    }

    mainWindow.width = dialog.width
    mainWindow.height = dialog.height
    mainWindow.moveMainWindowToScreenCenter()
    //mainWindow.title = dialog.title
    mainWindow.title = qsTr("BlockSettle Signer")

    dialog.dialogsChainFinished.connect(function(){ hide() })
    dialog.nextChainDialogChangedOverloaded.connect(function(nextDialog){
//        if (typeof nextDialog.qmlTitleVisible !== "undefined") {
//            nextDialog.qmlTitleVisible = false
//        }

        mainWindow.width = nextDialog.width
        mainWindow.height = nextDialog.height
        mainWindow.moveMainWindowToScreenCenter()

        nextDialog.sizeChanged.connect(function(w, h){
            mainWindow.width = w
            mainWindow.height = h
            mainWindow.moveMainWindowToScreenCenter()
        })
    })

    dialog.sizeChanged.connect(function(w, h){
        console.log("dialog.sizeChanged " + w + " " + h)
        mainWindow.width = w
        mainWindow.height = h
        mainWindow.moveMainWindowToScreenCenter()
    })
    raiseWindow(mainWindow)
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
        dlgCreateWallet.open()
    })
//    if (Object.keys(mainWindow).indexOf("currentDialog") != -1) {
//        mainWindow.sizeChanged.connect(function(w, h) {
//            dlgNewSeed.width = w
//            dlgNewSeed.height = h
//        })
//    }
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
    var dlg = Qt.createComponent("../BsDialogs/WalletBackupDialog.qml").createObject(mainWindow
            , {"walletInfo": qmlFactory.createWalletInfo(rootId)})

    // FIXME: save backups dir
    //dlg.targetDir = signerSettings.dirDocuments
    dlg.open()
    return dlg
}

function deleteWalletDialog(data) {
    var walletId = data["rootId"]
    var dlg = Qt.createComponent("../BsDialogs/WalletDeleteDialog.qml").createObject(mainWindow
            , {"rootName": walletsProxy.getRootWalletName(walletId),
               "walletInfo": qmlFactory.createWalletInfo(walletId)
              })

    dlg.open()
    return dlg
}

function manageEncryptionDialog(data) {
    var rootId = data["rootId"]
    var dlg = Qt.createComponent("../BsDialogs/WalletManageEncryptionDialog.qml").createObject(mainWindow
            , {"walletInfo": qmlFactory.createWalletInfo(rootId)})

    dlg.open()
    return dlg
}

function activateAutoSignDialog(data) {
    var walletId = data["rootId"]
    signerSettings.autoSignWallet = walletId
    signerStatus.activateAutoSign(walletId)
}

function createTxSignDialog(jsCallback, prompt, txInfo, passwordDialogData, walletInfo) {
    var dlg = Qt.createComponent("../BsDialogs/TxSignDialog.qml").createObject(mainWindow
            , {"prompt": prompt,
               "txInfo": txInfo,
               "passwordDialogData": passwordDialogData,
               "walletInfo": walletInfo
              })
    prepareLiteModeDialog(dlg)

    // FIXME: use bs error codes enum in qml
    dlg.bsAccepted.connect(function() {
        jsCallback(0, walletInfo.rootId, dlg.passwordData)
    })
    dlg.bsRejected.connect(function() {
        jsCallback(10, walletInfo.rootId, dlg.passwordData)
    })
    dlg.open()
    dlg.init()
}

function createSettlementTransactionDialog(jsCallback, prompt, txInfo, passwordDialogData, walletInfo) {
    var dlg = Qt.createComponent("../BsDialogs/SettlementTransactionDialog.qml").createObject(mainWindow
            , {"prompt": prompt,
               "txInfo": txInfo,
               "passwordDialogData": passwordDialogData,
               "walletInfo": walletInfo
              })
    prepareLiteModeDialog(dlg)

    // FIXME: use bs error codes enum in qml
    dlg.bsAccepted.connect(function() {
        jsCallback(0, walletInfo.rootId, dlg.passwordData)
    })
    dlg.bsRejected.connect(function() {
        jsCallback(10, walletInfo.rootId, dlg.passwordData)
    })
    dlg.open()
    dlg.init()
}

function createPasswordDialogForLeaf(jsCallback, passwordDialogData, walletInfo) {
    if (walletInfo.walletId === "") {
        jsCallback(10, walletInfo.walletId, {})
    }

    var dlg

    if (walletInfo.encType === QPasswordData.Auth) {
        dlg = requesteIdAuth(AutheIDClient.SignWallet, walletInfo, function(passwordData){
            jsCallback(0, walletInfo.walletId, passwordData)
        })
    }
    else if (walletInfo.encType === QPasswordData.Password){
        if (passwordDialogData.value("LeafDialogType") === "RequestPasswordForAuthLeaf") {
            dlg = Qt.createComponent("../BsControls/BSPasswordInputAuthLeaf.qml").createObject(mainWindow
                , {"walletInfo": walletInfo,
                   "passwordDialogData": passwordDialogData
                  })
        }
        else if (passwordDialogData.value("LeafDialogType") === "RequestPasswordForToken") {
            dlg = Qt.createComponent("../BsControls/BSPasswordInputToken.qml").createObject(mainWindow
                , {"walletInfo": walletInfo,
                   "passwordDialogData": passwordDialogData
                  })
        }

        dlg.open()
        dlg.bsAccepted.connect(function() {
            var passwordData = qmlFactory.createPasswordData()
            passwordData.encType = QPasswordData.Password
            passwordData.encKey = ""
            passwordData.textPassword = dlg.enteredPassword

            jsCallback(0, walletInfo.walletId, passwordData)
        })
    }

    prepareLiteModeDialog(dlg)
}

function isLiteMode(){
    return mainWindow.isLiteMode
}
