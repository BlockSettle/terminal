import QtQuick 2.9
import QtQuick.Dialogs 1.2

Loader {
    active: false
    property string startFromFolder
    property string dir
    property string title

    sourceComponent: FileDialog {
        visible: false
        title: title
        selectFolder: true
        folder: "file:///" + startFromFolder

        signal bsAccepted()

        onAccepted: {
            dir = qmlAppObj.getUrlPath(fileUrl)
            bsAccepted()
        }
    }
}
