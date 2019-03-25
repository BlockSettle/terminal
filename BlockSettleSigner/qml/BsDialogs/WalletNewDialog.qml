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
        Layout.leftMargin: 10
        Layout.rightMargin: 10

        CustomLabelValue{
            Layout.fillWidth: true
            text: qsTr("For guidance, please consult the <a href=\"https://autheid.com/\">Getting Started Guide</a>")
        }
    }

    cFooterItem: RowLayout {
        CustomButtonBar {
            Layout.fillWidth: true

            CustomButton {
                text: qsTr("Cancel")
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                onClicked: {
                    onClicked: root.close();
                }
            }

            CustomButton {
                id: importButon
                text: qsTr("Import Wallet")
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
