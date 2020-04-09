/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2

import "../StyledControls"

CustomTitleDialogWindow {
    id: root

    property int type: 0

    title: qsTr("New Wallet")
    width: 480
    rejectable: true

    enum WalletType {
        NewWallet = 1,
        ImportWallet = 2
    }

    cContentItem: RowLayout {
        width: parent.width
        spacing: 5
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.margins: 10

        CustomLabelValue{
            Layout.fillWidth: true
            text: qsTr("For guidance, please consult the <a href=\"https://pubb.blocksettle.com/PDF/BlockSettle%20Getting%20Started.pdf\">Getting Started Guide</a>")
        }
    }

    cFooterItem: ColumnLayout {
        spacing: 0
        Layout.margins: 0

        CustomButtonBar {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignBottom

            CustomButton {
                text: qsTr("Cancel")
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                onClicked: {
                    onClicked: rejectAnimated();
                }
            }

            CustomButton {
                id: importButon
                text: qsTr("Import Wallet / HW")
                anchors.right: createButon.left
                anchors.bottom: parent.bottom
                onClicked: {
                    type = WalletNewDialog.WalletType.ImportWallet
                    acceptAnimated()
                }
            }

            CustomButton {
                id: createButon
                text: qsTr("Create Wallet")
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                onClicked: {
                    type = WalletNewDialog.WalletType.NewWallet
                    acceptAnimated()
                }
            }
        }
    }
}
