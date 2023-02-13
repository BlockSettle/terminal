/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.0

import "../BsStyles"

// dialog window with header
CustomDialog {
    id: root
   // property bool qmlTitleVisible: true    //: !mainWindow.isLiteMode
    property alias headerPanel: headerPanel
    property bool fixedHeight: false
    property var customHeader: ColumnLayout {
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

    height: fixedHeight ? undefined: cHeaderHeight + cContentHeight + cFooterHeight

    function isApplicationWindow(item) {
        return item instanceof ApplicationWindow
    }

    cHeaderItem: customHeader
//    onNextChainDialogChangedOverloaded: {
//        nextDialog.qmlTitleVisible = qmlTitleVisible
//    }
}
