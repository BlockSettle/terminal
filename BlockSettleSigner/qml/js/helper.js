function toHex(str) {
    var hex = '';
    for(var i = 0; i < str.length; i++) {
        hex += ''+str.charCodeAt(i).toString(16);
    }
    return hex;
}

function openAbortBox(dialog, abortBoxType) {
    var abortBox = Qt.createComponent("../BsControls/BSAbortBox.qml").createObject(mainWindow)
    abortBox.abortType = abortBoxType
    abortBox.accepted.connect(function() {dialog.reject()} )
    abortBox.open()
}

function messageBox(type, title, text, details, parent) {
    var messageBox_ = Qt.createComponent("../BsControls/BSMessageBox.qml").createObject(mainWindow)

    messageBox_.type = type
    messageBox_.title = title
    messageBox_.customText = text
    if (details !== undefined) messageBox_.customDetails = details
    messageBox_.parent = parent
    messageBox_.cancelButtonVisible = false
    messageBox_.open()

    return messageBox_
}

function resultBox(type, result, walletInfo) {
    if (!result) return false   // only succeed result boxes used at the moment. Error shown directly from walletsProxy
    var messageBox_ = Qt.createComponent("../BsControls/BSResultBox.qml").createObject(mainWindow)

    messageBox_.walletInfo = walletInfo
    messageBox_.resultType = type
    messageBox_.result_ = result
    messageBox_.open()

    return messageBox_
}


function messageBoxInfo(title, text, details, parent) {
    messageBox(1, title, text, details, parent);
}

function messageBoxCritical(title, text, details, parent) {
    messageBox(4, title, text, details, parent);
}

function raiseWindow() {
    console.log("QML raiseWindow")
    mainWindow.show()
    mainWindow.raise()
    mainWindow.requestActivate()
    mainWindow.flags |= Qt.WindowStaysOnTopHint
    mainWindow.flags &= ~Qt.WindowStaysOnTopHint
}


// TODO refactot msgBoxes
//function changePwResultMsg(result, walletName) {
//    if (result) {
//        return messageBox(BSMessageBox.Type.Success
//                          , qsTr("Password")
//                          , qsTr("New password successfully set.")
//                          , qsTr("Wallet ID: %1").arg(walletName))
//    }
//    else {
//        return messageBox(BSMessageBox.Type.Critical
//                          , qsTr("Password")
//                          , qsTr("New password failed to set.")
//                          , qsTr("Wallet ID: %1").arg(walletName))
//    }
//}

//function createWalletResultMsg(result, walletInfo) {
//    if (result) {
//        return messageBox(BSMessageBox.Type.Success
//                          , qsTr("Wallet")
//                          , qsTr("Wallet successfully created.")
//                          , qsTr("Wallet ID: %1\nWallet name: %2").arg(walletInfo.walletId).arg(walletInfo.name))
//    }
//    else {
//        return messageBox(BSMessageBox.Type.Critical
//                          , qsTr("Wallet")
//                          , qsTr("New password failed to set.")
//                          , qsTr("Wallet ID: %1").arg(walletInfo.walletId))
//    }
//}

//function importWalletResultMsg(result, walletInfo) {
//    if (result) {
//        return messageBox(BSMessageBox.Type.Success
//                          , qsTr("Wallet")
//                          , qsTr("Wallet successfully imported.")
//                          , qsTr("Wallet ID: %1\nWallet name: %2").arg(walletInfo.walletId).arg(walletInfo.name))
//    }
//    else {
//        return messageBox(BSMessageBox.Type.Critical
//                          , qsTr("Wallet")
//                          , qsTr("New password failed to import.")
//                          , qsTr("Wallet ID: %1").arg(walletInfo.walletId))
//    }
//}

//function addDeviceResultMsg(result, walletInfo) {
//    if (result) {
//        return messageBox(BSMessageBox.Type.Success
//                          , qsTr("Wallet")
//                          , qsTr("Device successfully added.")
//                          , qsTr("Wallet ID: %1\nWallet name: %2").arg(walletInfo.walletId).arg(walletInfo.name))
//    }
//    else {
//        return messageBox(BSMessageBox.Type.Critical
//                          , qsTr("Wallet")
//                          , qsTr("New device failed to add.")
//                          , qsTr("Wallet ID: %1").arg(walletInfo.walletId))
//    }
//}

//function removeDeviceResultMsg(result, walletInfo) {
//    if (result) {
//        return messageBox(BSMessageBox.Type.Success
//                          , qsTr("Wallet")
//                          , qsTr("Device successfully removed.")
//                          , qsTr("Wallet ID: %1\nWallet name: %2").arg(walletInfo.walletId).arg(walletInfo.name))
//    }
//    else {
//        return messageBox(BSMessageBox.Type.Critical
//                          , qsTr("Wallet")
//                          , qsTr("Failed to remove device.")
//                          , qsTr("Wallet ID: %1").arg(walletInfo.walletId))
//    }
//}


function requesteIdAuth (requestType, walletInfo, onSuccess) {
    console.log("requesteIdAuth: " + walletInfo.walletId)
    var authObject = qmlFactory.createAutheIDSignObject(requestType, walletInfo)

    var authProgress = Qt.createComponent("../BsControls/BSEidProgressBox.qml").createObject(mainWindow);

    authProgress.title = qsTr("Password for wallet %1").arg(walletInfo.name)
    authProgress.customDetails = qsTr("Wallet ID: %1").arg(walletInfo.walletId)
    authProgress.customText = qsTr("%1").arg(walletInfo.email())
    authProgress.open()
    authProgress.rejected.connect(function() {
        authObject.destroy()
    })

    authObject.succeeded.connect(function(encKey_, password_) {
        authProgress.acceptAnimated()

        var passwordData = qmlFactory.createPasswordData()
        passwordData.encType = NsWallet.Auth
        passwordData.encKey = encKey_
        passwordData.binaryPassword = password_

        onSuccess(passwordData);
        authObject.destroy()
    })
    authObject.failed.connect(function(errorText) {
        console.log("authObject.failed.connect(function(errorText)) " + errorText)
        messageBox(BSMessageBox.Type.Critical
                                     , qsTr("Wallet")
                                     , qsTr("eID request failed with error: \n") + errorText
                                     , qsTr("Wallet Name: %1\nWallet ID: %2")
                                     .arg(walletInfo.name)
                                     .arg(walletInfo.walletId))

        authProgress.rejectAnimated()
        authObject.destroy()
    })
}

function removeEidDevice (index, walletInfo, onSuccess) {
    console.log("function removeEidDevice: " + walletInfo.walletId)

    var authObject = qmlFactory.createRemoveEidObject(index, walletInfo)

    var authProgress = Qt.createComponent("../BsControls/BSEidProgressBox.qml").createObject(mainWindow);

    authProgress.title = qsTr("Password for wallet %1").arg(walletInfo.name)
    authProgress.customDetails = qsTr("Wallet ID: %1").arg(walletInfo.walletId)
    authProgress.customText = qsTr("%1").arg(walletInfo.email())
    authProgress.open()
    authProgress.rejected.connect(function() {
        authObject.destroy()
    })

    authObject.succeeded.connect(function(encKey_, password_) {
        authProgress.acceptAnimated()

        var passwordData = qmlFactory.createPasswordData()
        passwordData.encType = NsWallet.Auth
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
                                     .arg(walletInfo.walletId))

        authProgress.rejectAnimated()
        authObject.destroy()
    })
}


function activateeIdAuth (email, walletInfo, onSuccess) {
    var authObject = qmlFactory.createActivateEidObject(email, walletInfo)

    var authProgress = Qt.createComponent("../BsControls/BSEidProgressBox.qml").createObject(mainWindow);

    authProgress.title = qsTr("Activate wallet")
    authProgress.customDetails = qsTr("Wallet ID: %1").arg(walletInfo.walletId)
    authProgress.customText = qsTr("%1").arg(email)
    authProgress.open()
    authProgress.rejected.connect(function() {
        authObject.destroy()
    })

    authObject.succeeded.connect(function(encKey_, password_) {
        authProgress.acceptAnimated()

        var passwordData = qmlFactory.createPasswordData()
        passwordData.encType = NsWallet.Auth
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
                                     .arg(walletInfo.walletId))

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
