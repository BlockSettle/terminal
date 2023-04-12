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
import "../BsControls"

BSWalletHandlerDialog {
    id: root

    property string headerButtonText: ""
    signal headerButtonClicked()

    customHeader: ColumnLayout {
        id: layout
        spacing: 0
        Layout.alignment: Qt.AlignTop
        Layout.margins: 0

        Rectangle {
            id: rect
            property string text : root.title
            clip: true
            color: "transparent"
            height: BSSizes.applyScale(40)

            Layout.fillWidth: true

            Rectangle {
                anchors.fill: rect
                color: BSStyle.dialogHeaderColor
            }

            Text {
                anchors.fill: rect
                leftPadding: BSSizes.applyScale(10)
                rightPadding: BSSizes.applyScale(10)

                text: rect.text
                font.capitalization: Font.AllUppercase
                color: BSStyle.textColor
                font.pixelSize: BSSizes.applyScale(11)
                verticalAlignment: Text.AlignVCenter
            }

            Button {
                id: btnExpand
                width: BSSizes.applyScale(100)
                anchors.right: rect.right
                anchors.top: rect.top
                anchors.bottom: rect.bottom

                contentItem: Text {
                    text: headerButtonText
                    color: BSStyle.textColor
                    font.pixelSize: BSSizes.applyScale(11)
                    font.underline: true
                    horizontalAlignment: Text.AlignRight
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideNone
                }

                background: Rectangle {
                    implicitWidth: BSSizes.applyScale(70)
                    implicitHeight: BSSizes.applyScale(35)
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
