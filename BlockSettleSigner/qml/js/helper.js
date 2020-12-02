/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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
    w.flags |= Qt.WindowStaysOnTopHint
    w.flags &= ~Qt.WindowStaysOnTopHint
    w.requestActivate()
}

function hideWindow(w) {
    if ( w.hasOwnProperty("currentDialog") &&
            (typeof w.currentDialog.hideMainWindow === "function")) {
        w.currentDialog.hideMainWindow();
        return;
    }

    w.hide()
}

function requesteIdAuth(requestType, walletInfo, authEidMessage, onSuccess) {
    var authObject = qmlFactory.createAutheIDSignObject(requestType, walletInfo, authEidMessage, authSign.defaultExpiration())
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
           , errorText
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
    authObject.cancelledByTimeout.connect(function() {
        console.log("QML requesteIdAuth: authObject.cancelledByTimeout")
        authProgress.rejectAnimated()
        authObject.destroy()
    })

    return authProgress
}

function addEidDevice(walletInfo, authEidMessage, onSuccess) {
    var authProgress = Qt.createComponent("../BsControls/BSEidProgressBox.qml").createObject(mainWindow)
    let qrCallback = function(code) {
        console.log("show QR code" + code)
        authProgress.qrCode = code
    }
    var authObject = qmlFactory.createAddEidObject(walletInfo, authEidMessage, qrCallback)

    authProgress.email = walletInfo.email()
    authProgress.walletId = walletInfo.rootId
    authProgress.walletName = walletInfo.name
    authProgress.requestType = AutheIDClient.ActivateWallet

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
           , errorText
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
    authObject.cancelledByTimeout.connect(function() {
        console.log("QML requesteIdAuth: authObject.cancelledByTimeout")
        authProgress.rejectAnimated()
        authObject.destroy()
    })

    return authProgress
}

function removeEidDevice (index, walletInfo, authEidMessage, onSuccess) {
    var authObject = qmlFactory.createRemoveEidObject(index, walletInfo, authEidMessage)
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
           , errorText
           , qsTr("Wallet Name: %1\nWallet ID: %2")
              .arg(walletInfo.name)
              .arg(walletInfo.rootId))

        authProgress.rejectAnimated()
        authObject.destroy()
    })
}


function activateeIdAuth(walletInfo, authEidMessage, onSuccess, onFailure) {
    var authProgress = Qt.createComponent("../BsControls/BSEidProgressBox.qml").createObject(mainWindow)
    let qrCallback = function(code) {
        console.log("show QR code" + code)
        authProgress.qrCode = code
    }
    var authObject = qmlFactory.createActivateEidObject(walletInfo.rootId, authEidMessage, qrCallback)

    authProgress.walletId = walletInfo.rootId
    authProgress.walletName = walletInfo.name
    authProgress.requestType = AutheIDClient.ActivateWallet

    authProgress.open()
    authProgress.bsRejected.connect(function() {
        if (authObject !== undefined) authObject.destroy()
        if (onFailure) {
            onFailure()
        }
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
           , errorText
           , qsTr("Wallet Name: %1\nWallet ID: %2")
              .arg(walletInfo.name)
              .arg(walletInfo.rootId))

        authProgress.rejectAnimated()
        authObject.destroy()
        if (onFailure) {
            onFailure()
        }
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
        console.log("helper.js evalWorker callback for: " + method)

        let cbArgList = new Array(7)

        if (typeof cbArg0 !== 'undefined') cbArgList[0] = cbArg0
        if (typeof cbArg1 !== 'undefined') cbArgList[1] = cbArg1
        if (typeof cbArg2 !== 'undefined') cbArgList[2] = cbArg2
        if (typeof cbArg3 !== 'undefined') cbArgList[3] = cbArg3
        if (typeof cbArg4 !== 'undefined') cbArgList[4] = cbArg4
        if (typeof cbArg5 !== 'undefined') cbArgList[5] = cbArg5
        if (typeof cbArg6 !== 'undefined') cbArgList[6] = cbArg6
        if (typeof cbArg7 !== 'undefined') cbArgList[7] = cbArg7

        if (typeof cppCallback === 'object' && cppCallback !== null) {
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

function prepareDialog(dialog) {
    console.log("[JsHelper] prepareDialog: " + dialog)

    if (dialog.hasOwnProperty("isPrepared")) {
        if (dialog.isPrepared) {
            return
        }
        else {
            dialog.isPrepared = true
        }
    }

    // close previous dialog
    if (currentDialog && typeof currentDialog.close !== "undefined") {
        currentDialog.close()
    }
    currentDialog = dialog

    if (isLiteMode()) {
        prepareLiteModeDialog(dialog)
    }
    else {
        prepareFullModeDialog(dialog)
    }
}

function prepareLiteModeDialog(dialog) {
    if (!isLiteMode()) {
        return
    }
    console.log("Prepare qml lite dialog")

    mainWindow.moveMainWindowToScreenCenter()
    mainWindow.resizeAnimated(dialog.width, dialog.height)

    mainWindow.title = qsTr("BlockSettle Signer")

    dialog.dialogsChainFinished.connect(function(){
        hide();
    })
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

function prepareFullModeDialog(dialog) {
    if (isLiteMode()) {
        return
    }
    console.log("Prepare qml full dialog")
    raiseWindow(mainWindow)

    let maxW = Math.max(dialog.width, mainWindow.width)
    let maxH = Math.max(dialog.height, mainWindow.height)
    if (maxW !== mainWindow.width || maxH !== mainWindow.height) {
        mainWindow.resizeAnimated(maxW, maxH)
    }

    dialog.sizeChanged.connect(function(w, h){
        let maxW = Math.max(w, mainWindow.width)
        let maxH = Math.max(h, mainWindow.height)
        if (maxW !== mainWindow.width || maxH !== mainWindow.height) {
            mainWindow.resizeAnimated(maxW, maxH)
        }
    })
}

// BST-2566: Skip prompting of Wallet encryption every time we a Create Wallet, only keep it as option in Setting
let publicEncryptionDisabledByDefault = true

function checkEncryptionPassword(dlg) {
    if (publicEncryptionDisabledByDefault) {
        prepareDialog(dlg);
        dlg.open();
        return dlg;
    }

    var onControlPasswordFinished = function(prevDialog, password){
        if (qmlFactory.controlPasswordStatus() === ControlPasswordStatus.RequestedNew) {
            walletsProxy.sendControlPassword(password)
            qmlFactory.setInitMessageWasShown();
            prevDialog.setNextChainDialog(dlg)
            prepareDialog(dlg);
            dlg.open()
            return;
        }
        else if (qmlFactory.controlPasswordStatus() === ControlPasswordStatus.Accepted) {
            return;
        }

        let onControlPasswordChanged = function(success, errorMsg){
            if (success) {
                qmlFactory.setControlPasswordStatus(ControlPasswordStatus.Accepted);
                prevDialog.setNextChainDialog(dlg)
                prepareDialog(dlg);
                dlg.open()
            } else {
                let mbFail= messageBox(BSMessageBox.Type.Critical
                    , qsTr("Public Data Encryption"), qsTr("Password update failed: \n") + errorMsg);
                mbFail.bsAccepted.connect(prevDialog.dialogsChainFinished)
                prevDialog.setNextChainDialog(mbFail)
            }
        }

        walletsProxy.changeControlPassword(password,
                                           password, onControlPasswordChanged);
    }

    if (qmlFactory.controlPasswordStatus() === ControlPasswordStatus.Rejected ||
            (qmlFactory.controlPasswordStatus() === ControlPasswordStatus.RequestedNew && !qmlFactory.initMessageWasShown())) {
        var controlPasswordDialog = createControlPasswordDialog(onControlPasswordFinished,
                                        qmlFactory.controlPasswordStatus(), true, false)
        return controlPasswordDialog
    }
    else {
        prepareDialog(dlg);
        dlg.open();
        return dlg;
    }
}

function createNewWalletDialog(data) {
    let newSeed = qmlFactory.createSeed(signerSettings.testNet)

    // allow user to save wallet seed lines and then prompt him to enter them for verification
    let dlgNewSeed = Qt.createComponent("../BsDialogs/WalletNewSeedDialog.qml").createObject(mainWindow)
    dlgNewSeed.seed = newSeed
    dlgNewSeed.bsAccepted.connect(function() {
        // let user set a password or Auth eID and also name and desc. for the new wallet
        var dlgCreateWallet = Qt.createComponent("../BsDialogs/WalletCreateDialog.qml").createObject(mainWindow)
        dlgNewSeed.setNextChainDialog(dlgCreateWallet)
        dlgCreateWallet.seed = newSeed
        prepareDialog(dlgCreateWallet);
        dlgCreateWallet.open()
    })

    return checkEncryptionPassword(dlgNewSeed);
}

function importWalletDialog(data) {
    let dlgImp = Qt.createComponent("../BsDialogs/WalletImportDialog.qml").createObject(mainWindow)
    return checkEncryptionPassword(dlgImp);
}

function importHwWalletDialog(data) {
    let dlgImp = Qt.createComponent("../BsDialogs/WalletImportHwDialog.qml").createObject(mainWindow)
    return checkEncryptionPassword(dlgImp);
}

function managePublicDataEncryption() {
    const previousState = qmlFactory.controlPasswordStatus();

    let onControlPasswordFinished = function(dialog, newPassword, oldPassword){
        if (previousState === ControlPasswordStatus.RequestedNew) {
            walletsProxy.sendControlPassword(newPassword);
            qmlFactory.setInitMessageWasShown();
            if (newPassword !== "") {
                let mbAccept = messageBox(BSMessageBox.Type.Success
                    , qsTr("Public Data Encryption"), qsTr("Password has successfully been set"));
                mbAccept.bsAccepted.connect(dialog.dialogsChainFinished);
                dialog.setNextChainDialog(mbAccept)
            } else {
                dialog.dialogsChainFinished();
            }

            return;
        }

        let successMessageBody;
        let errorMessageBody;
        let updatedOldPassword;
        if (previousState === ControlPasswordStatus.Accepted) {
            successMessageBody = qsTr("Password has successfully been changed");
            errorMessageBody = qsTr("Password update failed: ");
            updatedOldPassword = oldPassword;
        } else if (previousState === ControlPasswordStatus.Rejected) {
            successMessageBody = qsTr("Password has successfully been set");
            errorMessageBody = qsTr("Password set failed: ");
            updatedOldPassword = newPassword;
        }

        let onControlPasswordChanged = function(success, errorMsg){
            if (success) {
                qmlFactory.setControlPasswordStatus(ControlPasswordStatus.Accepted);
                let mbSuccess = messageBox(BSMessageBox.Type.Success
                    , qsTr("Public Data Encryption"), qsTr(successMessageBody));
                mbSuccess.bsAccepted.connect(dialog.dialogsChainFinished)
                dialog.setNextChainDialog(mbSuccess)
                if (newPassword.textPassword.length === 0) {
                    qmlFactory.setControlPasswordStatus(ControlPasswordStatus.RequestedNew);
                }
            } else {
                let mbFail= messageBox(BSMessageBox.Type.Critical
                    , qsTr("Public Data Encryption"), qsTr(errorMessageBody + "\n") + errorMsg);
                mbFail.bsAccepted.connect(dialog.dialogsChainFinished)
                dialog.setNextChainDialog(mbFail)
            }
        }

        walletsProxy.changeControlPassword(updatedOldPassword,
                                           newPassword, onControlPasswordChanged);
    }

    let controlPasswordDialog = createControlPasswordDialog(onControlPasswordFinished,
                                        qmlFactory.controlPasswordStatus(), false, false)
    return controlPasswordDialog;
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

    var dlg = Qt.createComponent("../BsControls/BSPasswordInputAutoSignDialog.qml").createObject(mainWindow
        , {"walletInfo": walletInfo});

    prepareDialog(dlg)
    dlg.open()
    dlg.init()

    dlg.bsAccepted.connect(function() {
        signerStatus.activateAutoSign(walletInfo.rootId, dlg.passwordData, true, autoSignCallback)
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
    prepareDialog(dlg)

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

    dlg.bsAccepted.connect(function() {
        jsCallback(qmlFactory.errorCodeNoError(), walletInfo.rootId, dlg.passwordData)
    })
    dlg.bsRejected.connect(function() {
        jsCallback(qmlFactory.errorCodeTxCanceled(), walletInfo.rootId, dlg.passwordData)
    })
    dlg.open()
    dlg.init()

    prepareDialog(dlg)
}

function createPasswordDialogForType(jsCallback, passwordDialogData, walletInfo) {
    console.log("helper.js createPasswordDialogForType, dialogType: " + passwordDialogData.DialogType
       + ", walletId: " + walletInfo.walletId)

    if (walletInfo.walletId === "") {
        jsCallback(qmlFactory.errorCodeTxCanceled(), walletInfo.walletId, {})
    }

    let dlg = null;

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
    else if (passwordDialogData.DialogType === "RequestPasswordForPromoteWallet") {
        dlg = Qt.createComponent("../BsControls/BSPasswordInputPromoteWallet.qml").createObject(mainWindow
            , {"walletInfo": walletInfo,
               "passwordDialogData": passwordDialogData
              })
    }

    prepareDialog(dlg)

    dlg.bsAccepted.connect(function() {
        jsCallback(qmlFactory.errorCodeNoError(), walletInfo.walletId, dlg.passwordData)
    })

    dlg.bsRejected.connect(function() {
        jsCallback(qmlFactory.errorCodeTxCanceled(), walletInfo.rootId, dlg.passwordData)
    })

    dlg.open()
    dlg.init()
}

function updateDialogData(jsCallback, passwordDialogData) {
    console.log("Trying to update password dialog with Settl Id: " + passwordDialogData.SettlementId)
    if (!currentDialog || typeof currentDialog.passwordDialogData === "undefined") {
        console.log("Warning: current dialog not set")
        return
    }
    console.log("Current dialog with Settl Id: " + currentDialog.passwordDialogData.SettlementId)

    if (passwordDialogData.SettlementId === currentDialog.passwordDialogData.SettlementId) {
        console.log("Updating password dialog, updated keys: " + passwordDialogData.keys())
        currentDialog.passwordDialogData.merge(passwordDialogData)
    }
}

function createControlPasswordDialog(jsCallback, controlPasswordStatus, usedInChain, initDialog) {
    let dlg = Qt.createComponent("../BsControls/BSControlPasswordInput.qml").createObject(mainWindow
        , { "controlPasswordStatus": controlPasswordStatus ,
            "usedInChain" : usedInChain ,
            "initDialog" : initDialog });

    if (controlPasswordStatus === ControlPasswordStatus.Accepted) {
        dlg.bsAccepted.connect(function() {
            jsCallback(dlg, dlg.passwordData, dlg.passwordDataOld)
        })
    }
    else {
        dlg.bsAccepted.connect(function() {
            jsCallback(dlg, dlg.passwordData)
        })

        if (usedInChain) {
            dlg.bsRejected.connect(function() {
                jsCallback(dlg, "")
            })
        }
    }

    prepareDialog(dlg)
    dlg.open()
    return dlg
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

function getAuthEidMessageLine(key, value, isLastLine) {
    if (value === "")
        return "";

    let result = key + ': ' + value;
    if ((typeof(isLastLine) !== undefined) && !isLastLine)
        result += '\n';

    return result;
}

function getAuthEidWalletInfo(walletInfo) {
    return getAuthEidMessageLine("Wallet Name", walletInfo.name)
            + getAuthEidMessageLine("Wallet ID", walletInfo.walletId, true);
}

function getAuthEidTransactionInfo(txInfo) {
    let result =
        getAuthEidMessageLine("Input Amount", txInfo.inputAmount.toFixed(8)) +
        getAuthEidMessageLine("Return Amount", txInfo.changeAmount.toFixed(8)) +
        getAuthEidMessageLine("Transaction fee", txInfo.fee.toFixed(8)) +
        getAuthEidMessageLine("Transaction amount", txInfo.amount.toFixed(8)) +
        getAuthEidMessageLine("Total spent", txInfo.total.toFixed(8));

    result += "Output address(es):\n";
    for (let i = 0; i < txInfo.allRecipients.length; ++i) {
        result += txInfo.allRecipients[i];
        if (i + 1 !== txInfo.allRecipients.length)
            result += "\n";
    }

    return result;
}

function getAuthEidSettlementInfo(product, priceString, is_sell, quantity, totalValue) {
    return JsHelper.getAuthEidMessageLine("Product", product) +
        JsHelper.getAuthEidMessageLine("Price", priceString) +
        JsHelper.getAuthEidMessageLine("Quantity", quantity) +
        JsHelper.getAuthEidMessageLine("Deliver", (is_sell ? quantity : totalValue)) +
        JsHelper.getAuthEidMessageLine("Receive", (is_sell ? totalValue : quantity), true);
}

function showHwPinMatrix(deviceIndex) {
    let pinMatrix = Qt.createComponent("../BsHw/PinMatrixDialog.qml").createObject(mainWindow);

    pinMatrix.deviceIndex = deviceIndex;
    pinMatrix.open();

    return pinMatrix;
}

function showHwPassphrase(deviceIndex, allowedOnDevice) {
    let passphrase = Qt.createComponent("../BsHw/PassphraseDialog.qml").createObject(mainWindow);

    passphrase.deviceIndex = deviceIndex;
    passphrase.allowedOnDevice = allowedOnDevice;
    passphrase.open();

    return passphrase;
}

function showDropHwDeviceMessage() {
    return JsHelper.messageBox(BSMessageBox.Type.Warning
        , qsTr("Hardware wallet transaction signing")
        , qsTr("Cancelling transaction on device")
        , qsTr("The signer cannot force the device to drop the current transaction due to device specification. " +
               "Please ensure that the transaction is manually rejected on your device before making further transactions"));
}
