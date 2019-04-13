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
    if (type !==BSMessageBox.Type.Question) messageBox_.cancelButtonVisible = false
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

function raiseWindow() {
    mainWindow.show()
    mainWindow.raise()
    mainWindow.requestActivate()
    mainWindow.flags |= Qt.WindowStaysOnTopHint
    mainWindow.flags &= ~Qt.WindowStaysOnTopHint
}

function hideWindow() {
    mainWindow.hide()
}

function requesteIdAuth (requestType, walletInfo, onSuccess) {
    var authObject = qmlFactory.createAutheIDSignObject(requestType, walletInfo)
    var authProgress = Qt.createComponent("../BsControls/BSEidProgressBox.qml").createObject(mainWindow);

    authProgress.email = walletInfo.email()
    authProgress.walletId = walletInfo.rootId
    authProgress.walletName = walletInfo.name

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
    authObject.userCancelled.connect(function() {
        authProgress.rejectAnimated()
        authObject.destroy()
    })
}

function removeEidDevice (index, walletInfo, onSuccess) {
    var authObject = qmlFactory.createRemoveEidObject(index, walletInfo)
    var authProgress = Qt.createComponent("../BsControls/BSEidProgressBox.qml").createObject(mainWindow);

    authProgress.email = walletInfo.email()
    authProgress.walletId = walletInfo.rootId
    authProgress.walletName = walletInfo.name

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
