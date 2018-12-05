import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2


CustomDialog {
    property int type: 0
    id: root
    implicitWidth: parent.width * 0.9
    implicitHeight: mainLayout.implicitHeight

    enum WalletType {
        NewWallet = 1,
        ImportWallet = 2
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10
        width: parent.width
        id: mainLayout

        RowLayout{
            CustomHeaderPanel{
                Layout.preferredHeight: 40
                id: panelHeader
                Layout.fillWidth: true
                text:   qsTr("New Wallet")

            }
        }

        RowLayout {
            width: parent.width
            spacing: 5
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            CustomLabelValue{
                Layout.fillWidth: true
                text:   qsTr("Please select where you would like to create a new wallet from...")
            }
        }

        CustomButtonBar {
            implicitHeight: childrenRect.height
            implicitWidth: root.width
            id: rowButtons

            Flow {
                id: buttonRow
                spacing: 5
                padding: 5
                height: childrenRect.height + 10
                width: parent.width - buttonRowLeft - 5
                LayoutMirroring.enabled: true
                LayoutMirroring.childrenInherit: true
                anchors.left: parent.left   // anchor left becomes right


                CustomButton {
                    Layout.fillWidth: true
                    text:   qsTr("Import Wallet")
                    onClicked: {
                        type = WalletNewDialog.WalletType.ImportWallet
                        accept()
                    }
                }
                CustomButton {
                    Layout.fillWidth: true
                    text:   qsTr("Create New")
                    onClicked: {
                        type = WalletNewDialog.WalletType.NewWallet
                        accept()
                    }
                }

            }

            Flow {
                id: buttonRowLeft
                spacing: 5
                padding: 5
                height: childrenRect.height + 10


                CustomButton {
                    Layout.fillWidth: true
                    text:   qsTr("Cancel")
                    onClicked: {
                        onClicked: root.close();
                    }
                }

            }
        }
    }
}
