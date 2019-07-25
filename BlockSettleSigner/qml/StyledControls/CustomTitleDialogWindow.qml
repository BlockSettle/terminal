import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.0

import "../BsStyles"

// dialog window with header
CustomDialog {
    id: root
    property bool qmlTitleVisible: !mainWindow.isLiteMode
    property alias headerPanel: headerPanel
    property int headerPanelHeight: 40
    //property int headerPanelHeight: qmlTitleVisible ? 40 : 0
    //height: eval("layout.height + 200 + cFooterHeight")
    height: cHeaderHeight + cContentHeight + cFooterHeight

    function isApplicationWindow(item) {
        return item instanceof ApplicationWindow
    }

    Component.onCompleted: {
        //                console.log("parent " + (parent.parent.parent.parent.parent.parent === null))
        //                console.log("parent " + (parent.parent.parent.parent.parent.parent == 0))
        //                console.log("parent " + (parent.parent.parent.parent.parent.parent === 0))
        //                console.log("parent " + (parent.parent.parent.parent.parent.parent == undefined))
        //                console.log("parent " + (parent.parent.parent.parent.parent.parent === undefined))

        //console.log("parent " + isApplicationWindow(parent))
        //console.log("mainWindow " + isApplicationWindow(mainWindow))
    }

    cHeaderItem: ColumnLayout {
        id: layout
        spacing: 0
        Layout.alignment: Qt.AlignTop
        Layout.margins: 0

        CustomHeaderPanel {
            id: headerPanel
            Layout.fillWidth: true
            Layout.preferredHeight: {
//                console.log("parent " + parent.parent.parent.parent.parent.parent.parent.parent)
//                console.log("parent " + (parent.parent.parent.parent.parent.parent.parent.parent == null))


                return (mainWindow.isLiteMode && (parent.parent.parent.parent.parent.parent === null)) ? 0 : 40

            }
            //height: 40
            //implicitHeight: 40
            text: root.title
            //visible: qmlTitleVisible
        }
    }

    onNextChainDialogChangedOverloaded: {
        nextDialog.qmlTitleVisible = qmlTitleVisible
    }
}
