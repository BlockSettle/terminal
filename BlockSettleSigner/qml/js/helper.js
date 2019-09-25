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
    if ( w.hasOwnProperty("currentDialog") &&
            (typeof w.currentDialog.rejectAnimated === "function")) {
        w.currentDialog.rejectAnimated();
        return;
    }

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
    // NOTE: activateAutoSignDialog won't return dialog
    return dlg
}

function evalWorker(method, cppCallback, argList) {
    console.log("helper.js evalWorker call: " + method)

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

        if (cppCallback) {
            cppCallback.exec(cbArgList)
        }
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
        raiseWindow(mainWindow)
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

    mainWindow.moveMainWindowToScreenCenter()
    mainWindow.resizeAnimated(dialog.width, dialog.height)

    mainWindow.title = qsTr("BlockSettle Signer")

    dialog.dialogsChainFinished.connect(function(){ hide() })
    dialog.nextChainDialogChangedOverloaded.connect(function(nextDialog){
//        if (typeof nextDialog.qmlTitleVisible !== "undefined") {
//            nextDialog.qmlTitleVisible = false
//        }

        mainWindow.moveMainWindowToScreenCenter()
        mainWindow.resizeAnimated(nextDialog.width, nextDialog.height)

        nextDialog.sizeChanged.connect(function(w, h){
            mainWindow.moveMainWindowToScreenCenter()
            mainWindow.resizeAnimated(w, h)
        })
    })

    dialog.sizeChanged.connect(function(w, h){
        // console.log("helper.js dialog.sizeChanged " + w + " " + h)
        mainWindow.moveMainWindowToScreenCenter()
        mainWindow.resizeAnimated(w, h)
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
    var enable = data["enable"]
    tryChangeAutoSign(enable, walletId, false)
}

function tryActivateAutoSign(walletInfo, showResult) {
    var autoSignCallback = function(success, errorMsg) {
        if (!showResult) {
            return
        }

        if (success) {
            JsHelper.messageBox(BSMessageBox.Type.Success
                , qsTr("Wallet Auto Sign")
                , qsTr("Auto Signing enabled for wallet %1").arg(walletInfo.rootId))
        } else {
            JsHelper.messageBox(BSMessageBox.Type.Critical
                , qsTr("Wallet Auto Sign")
                , qsTr("Failed to enable auto signing.")
                , errorString)
        }
    }

    var passwordDialog = Qt.createComponent("../BsControls/BSPasswordInputAutoSignDialog.qml").createObject(mainWindow
        , {"walletInfo": walletInfo});

    prepareLiteModeDialog(passwordDialog)
    passwordDialog.open()
    passwordDialog.bsAccepted.connect(function() {
        var passwordData = qmlFactory.createPasswordData()
        passwordData.encType = QPasswordData.Password
        passwordData.encKey = ""
        passwordData.textPassword = passwordDialog.enteredPassword

        signerStatus.activateAutoSign(walletInfo.rootId, passwordData, true, autoSignCallback)
    })
}

function tryDeactivateAutoSign(walletInfo, showResult) {
    var autoSignDisableCallback = function(success, errorMsg) {
        if (!showResult) {
            return
        }

        if (success) {
            JsHelper.messageBox(BSMessageBox.Type.Success
                , qsTr("Wallet Auto Sign")
                , qsTr("Auto Signing disabled for wallet %1")
                    .arg(walletInfo.rootId))
        }
        else {
            JsHelper.messageBox(BSMessageBox.Type.Critical
                , qsTr("Wallet Auto Sign")
                , qsTr("Failed to disable auto signing.")
                , errorString)
        }
    }

    signerStatus.activateAutoSign(walletInfo.rootId, 0, false, autoSignDisableCallback)
}

function tryChangeAutoSign(newState, walletId, showResult) {
    var walletInfo = qmlFactory.createWalletInfo(walletId)

    if (newState) {
        tryActivateAutoSign(walletInfo, showResult)
    }
    else {
        tryDeactivateAutoSign(walletInfo, showResult)
    }
}

function createTxSignDialog(jsCallback, txInfo, passwordDialogData, walletInfo) {
    var dlg = Qt.createComponent("../BsDialogs/TxSignDialog.qml").createObject(mainWindow
            , {"txInfo": txInfo,
               "passwordDialogData": passwordDialogData,
               "walletInfo": walletInfo
              })
    prepareLiteModeDialog(dlg)

    dlg.bsAccepted.connect(function() {
        jsCallback(qmlFactory.errorCodeNoError(), walletInfo.rootId, dlg.passwordData)
    })
    dlg.bsRejected.connect(function() {
        jsCallback(qmlFactory.errorCodeTxCanceled(), walletInfo.rootId, dlg.passwordData)
    })
    dlg.open()
    dlg.init()
}

function createTxSignSettlementDialog(jsCallback, txInfo, passwordDialogData, walletInfo) {
    var dlg = null
    if (passwordDialogData.Market === "XBT") {
        dlg = Qt.createComponent("../BsDialogs/TxSignSettlementXBTMarketDialog.qml").createObject(mainWindow
           , {"txInfo": txInfo,
              "passwordDialogData": passwordDialogData,
               "walletInfo": walletInfo
        })
    }
    else if (passwordDialogData.Market === "CC") {
        dlg = Qt.createComponent("../BsDialogs/TxSignSettlementCCMarketDialog.qml").createObject(mainWindow
           , {"txInfo": txInfo,
              "passwordDialogData": passwordDialogData,
               "walletInfo": walletInfo
        })
    }
    else {
        console.log("[helper.js] Error: passwordDialogData.Market) is not set, dialog not created")
        return
    }

    prepareLiteModeDialog(dlg)

    dlg.bsAccepted.connect(function() {
        jsCallback(qmlFactory.errorCodeNoError(), walletInfo.rootId, dlg.passwordData)
    })
    dlg.bsRejected.connect(function() {
        jsCallback(qmlFactory.errorCodeTxCanceled(), walletInfo.rootId, dlg.passwordData)
    })
    dlg.open()
    dlg.init()
}

function createPasswordDialogForType(jsCallback, passwordDialogData, walletInfo) {
    console.log("helper.js createPasswordDialogForType, dialogType: " + passwordDialogData.DialogType
       + ", walletId: " + walletInfo.walletId)

    if (walletInfo.walletId === "") {
        jsCallback(qmlFactory.errorCodeTxCanceled(), walletInfo.walletId, {})
    }

    let dlg = null;
    if (walletInfo.encType === QPasswordData.Auth) {
        dlg = requesteIdAuth(AutheIDClient.SignWallet, walletInfo, function(passwordData){
            jsCallback(qmlFactory.errorCodeNoError(), walletInfo.walletId, passwordData)
        })
    }
    else if (walletInfo.encType === QPasswordData.Password){
        if (passwordDialogData.DialogType === "RequestPasswordForAuthLeaf") {
            dlg = Qt.createComponent("../BsControls/BSPasswordInputAuthLeaf.qml").createObject(mainWindow
                , {"walletInfo": walletInfo,
                   "passwordDialogData": passwordDialogData
                  })
        }
        else if (passwordDialogData.DialogType === "RequestPasswordForToken") {
            dlg = Qt.createComponent("../BsControls/BSPasswordInputToken.qml").createObject(mainWindow
                , {"walletInfo": walletInfo,
                   "passwordDialogData": passwordDialogData
                  })
        }
        else if (passwordDialogData.DialogType === "RequestPasswordForSettlementLeaf") {
            dlg = Qt.createComponent("../BsControls/BSPasswordInputSettlementLeaf.qml").createObject(mainWindow
                , {"walletInfo": walletInfo,
                   "passwordDialogData": passwordDialogData
                  })
        }
        else if (passwordDialogData.DialogType === "RequestPasswordForRevokeAuthAddress") {
            dlg = Qt.createComponent("../BsControls/BSPasswordInputRevokeAuthAddress.qml").createObject(mainWindow
                , {"walletInfo": walletInfo,
                   "passwordDialogData": passwordDialogData
                  })
        }
        else if (passwordDialogData.DialogType === "RequestPasswordForPromoteHDWallet") {
            dlg = Qt.createComponent("../BsControls/BSPasswordInputPromoteWallet.qml").createObject(mainWindow
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

            jsCallback(qmlFactory.errorCodeNoError(), walletInfo.walletId, passwordData)
        })
    }

    prepareLiteModeDialog(dlg)
}

function updateDialogData(jsCallback, passwordDialogData) {
    console.log("Updating password dialog " + currentDialog + ", updated keys: " + passwordDialogData.keys())
    if (!currentDialog || typeof currentDialog.passwordDialogData === "undefined") {
        return
    }
    currentDialog.passwordDialogData.merge(passwordDialogData)
}

function isLiteMode(){
    return mainWindow.isLiteMode
}

function truncString(string, maxLength) {
    if (!string) return string
    if (maxLength < 1) return string
    if (string.length <= maxLength) return string
    if (maxLength === 1) return string.substring(0,1) + '...'

    var midpoint = Math.ceil(string.length / 2)
    var toremove = string.length - maxLength
    var lstrip = Math.ceil(toremove/2)
    var rstrip = toremove - lstrip
    return string.substring(0, midpoint-lstrip) + '...'  + string.substring(midpoint+rstrip)
}

String.prototype.truncString = function(maxLength){
   return truncString(this, maxLength)
}

function initJSDialogs() {
    let folderListObj = Qt.createQmlObject(
        'import Qt.labs.folderlistmodel 2.12;FolderListModel { property bool isReady: status === FolderListModel.Ready; folder : "../BsDialogs/"; }'
        , mainWindow);

    folderListObj.statusChanged.connect(function() {
        if (!folderListObj.isReady)
            return
        for (let i = 0; i < folderListObj.count; ++i) {
            if (folderListObj.get(i, "fileSuffix") !== 'qml') {
                continue;
            }

            let fileName = folderListObj.get(i, "fileName");
            let childComp = Qt.createComponent("../BsDialogs/" + fileName);
            let childObj = childComp.incubateObject(null);
            if (childObj.status !== Component.Ready) {
                childObj.onStatusChanged = function(status) {
                    if (status === Component.Ready) {
                        childObj.object.destroy();
                    }
                }
            } else {
                childObj.object.destroy();
            }
        }
    })
}
