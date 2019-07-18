import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.0

import "../BsStyles"

// dialog window with header
CustomDialog {
    id: root
    property bool qmlTitleVisible: true
    property int headerPanelHeight: qmlTitleVisible ? 40 : 0

    cHeaderItem: RowLayout {
        CustomHeaderPanel {
            id: panel
            Layout.fillWidth: true
            Layout.preferredHeight: root.headerPanelHeight
            text: root.title
            visible: qmlTitleVisible
        }
    }

    onNextChainDialogChangedOverloaded: {
        nextDialog.qmlTitleVisible = qmlTitleVisible
    }
}
