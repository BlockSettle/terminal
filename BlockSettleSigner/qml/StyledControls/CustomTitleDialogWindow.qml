import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.0

import "../BsStyles"

// dialog window with header
CustomDialog {
    id: root
   // property bool qmlTitleVisible: true    //: !mainWindow.isLiteMode
    property alias headerPanel: headerPanel
    height: cHeaderHeight + cContentHeight + cFooterHeight

    function isApplicationWindow(item) {
        return item instanceof ApplicationWindow
    }

    cHeaderItem: ColumnLayout {
        id: layout
        spacing: 0
        Layout.alignment: Qt.AlignTop
        Layout.margins: 0

        CustomHeaderPanel {
            id: headerPanel
            Layout.fillWidth: true
            //qmlTitleVisible: root.qmlTitleVisible
            text: root.title
        }
    }

//    onNextChainDialogChangedOverloaded: {
//        nextDialog.qmlTitleVisible = qmlTitleVisible
//    }
}
