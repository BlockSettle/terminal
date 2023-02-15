import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15
import QtQuick.Dialogs 1.0

import "../BsStyles"
import "../StyledControls"

ColumnLayout  {
    id: layout

    property var wallet_properties_vm

    height: 548
    width: 580

    spacing: 0

    CustomTitleLabel {
        id: title
        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : title.height
        text: qsTr("Export watching-only wallet")
    }

    Item {
        width: parent.width
        height: 24
    }

    Rectangle {
        width: 530
        height: 82
        radius: 14
        color: BSStyle.exportWalletLabelBackground
        Layout.alignment: Qt.AlignCenter

        Grid {
            columns: 2
            rowSpacing: 14
            width: parent.width
            anchors.centerIn: parent

            Text {
                text: qsTr("Wallet name")
                color: BSStyle.exportWalletLabelNameColor
                font.family: "Roboto"
                font.pixelSize: 14
                width: parent.width / 2
                leftPadding: 20
            }
            Text {
                text: wallet_properties_vm.walletName
                color: BSStyle.textColor
                font.family: "Roboto"
                font.pixelSize: 14
                width: parent.width / 2
                horizontalAlignment: Text.AlignRight
                rightPadding: 20
            }

            Text {
                text: qsTr("Wallet ID")
                color: BSStyle.exportWalletLabelNameColor
                font.family: "Roboto"
                font.pixelSize: 14
                width: parent.width / 2
                leftPadding: 20
            }
            Text {
                text: wallet_properties_vm.walletId
                color: BSStyle.textColor
                font.family: "Roboto"
                font.pixelSize: 14
                width: parent.width / 2
                horizontalAlignment: Text.AlignRight
                rightPadding: 20
            }
        }
    }

    Item {
        width: parent.width
        height: 32
    }

    CustomTitleLabel {
        width: 530
        font.pixelSize: 14
        text: qsTr("Backup file:")
        leftPadding: 24
    }

    CustomTitleLabel {
        font.pixelSize: 14
        color: BSStyle.textColor
        text: wallet_properties_vm.exportPath
        leftPadding: 24
    }

    CustomButtonLeftIcon {
        Layout.leftMargin: 24
        width: 160
        text: qsTr("Select target dir")
        font.pixelSize: 12

        custom_icon.source: "qrc:/images/folder_icon.png"
        onClicked: fileDialog.open()
    }

    Label {
        id: spacer
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    CustomButton {
        id: confirm_but
        preferred: true
        text: qsTr("Export")

        Layout.bottomMargin: 40
        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

        width: 530

        function click_enter() {
            bsApp.exportWallet()
        }
    }

    FileDialog {
        id: fileDialog
        title: qsTr("Please choose a directory")
        folder: shortcuts.home
        selectFolder: true

        onAccepted: {
            wallet_properties_vm.exportPath = fileDialog.fileUrl
        }
    }
}
