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
    messageBox_.customDetails = details
    messageBox_.parent = parent
    messageBox_.cancelButtonVisible = false
    messageBox_.open()
}

function messageBoxInfo(title, text, details, parent) {
    messageBox(1, title, text, details, parent);
}

function messageBoxCritical(title, text, details, parent) {
    messageBox(3, title, text, details, parent);
}

function raiseWindow() {
    console.log("QML raiseWindow")
     mainWindow.show()
     mainWindow.raise()
     mainWindow.requestActivate()
     mainWindow.flags |= Qt.WindowStaysOnTopHint
     mainWindow.flags &= ~Qt.WindowStaysOnTopHint
}
