import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.0

import "../BsStyles"

// dialog window with header
CustomDialog {
    id: root
    property bool qmlTitleVisible: true
    cHeaderItem: RowLayout {
        CustomHeaderPanel {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            text: root.title
            visible: qmlTitleVisible
        }
    }
}
