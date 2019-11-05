import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.0

import "../BsStyles"

// dialog window with header
CustomDialog {
    id: root
    property bool fixedHeight: false
    property string headerButtonText: ""
    height: fixedHeight ? undefined: cHeaderHeight + cContentHeight + cFooterHeight

    signal headerButtonClicked()

    cHeaderItem: ColumnLayout {
        id: layout
        spacing: 0
        Layout.alignment: Qt.AlignTop
        Layout.margins: 0

        Rectangle {
            id: rect
            property string text : root.title
            clip: true
            color: "transparent"
            height: 40

            Layout.fillWidth: true

            Rectangle {
                anchors.fill: rect
                color: BSStyle.dialogHeaderColor
            }

            Text {
                anchors.fill: rect
                leftPadding: 10
                rightPadding: 10

                text: rect.text
                font.capitalization: Font.AllUppercase
                color: BSStyle.textColor
                font.pixelSize: 11
                verticalAlignment: Text.AlignVCenter
            }

            Button {
                id: btnExpand
                width: 100
                anchors.right: rect.right
                anchors.top: rect.top
                anchors.bottom: rect.bottom

                contentItem: Text {
                    text: headerButtonText
                    color: BSStyle.textColor
                    font.pixelSize: 11
                    font.underline: true
                    horizontalAlignment: Text.AlignRight
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideNone
                }

                background: Rectangle {
                    implicitWidth: 70
                    implicitHeight: 35
                    color: "transparent"
                }

                MouseArea {
                  anchors.fill: parent
                  enabled: false
                  cursorShape: Qt.PointingHandCursor
                }

                onClicked: headerButtonClicked()
            }
        }
    }
}
