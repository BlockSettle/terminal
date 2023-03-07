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

    Rectangle {
        width: 530
        height: 82
        radius: 14
        color: BSStyle.exportWalletLabelBackground

        Layout.topMargin: 24
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

    Label {

        Layout.leftMargin: 24
        Layout.rightMargin: 24
        Layout.topMargin: 32
        Layout.preferredHeight: 16
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignLeft | Qt.AlingTop

        font.pixelSize: 14
        font.family: "Roboto"
        font.weight: Font.Normal

        text: qsTr("Backup file:")
        color: BSStyle.wildBlueColor
    }

    Label {

        Layout.leftMargin: 24
        Layout.rightMargin: 24
        Layout.topMargin: 8
        Layout.preferredHeight: 16
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignLeft | Qt.AlingTop

        font.pixelSize: 14
        color: BSStyle.titanWhiteColor
        text: wallet_properties_vm.exportPath
        leftPadding: 24
    }

    Button {

        Layout.leftMargin: 24
        Layout.topMargin: 12
        Layout.alignment: Qt.AlignLeft | Qt.AlingTop

        activeFocusOnTab: false

        font.pixelSize: 13
        font.family: "Roboto"
        font.weight: Font.Normal
        palette.buttonText: BSStyle.buttonsHeaderTextColor

        text: qsTr("Select target dir")

        icon.color: BSStyle.wildBlueColor
        icon.source: "qrc:/images/folder_icon.png"
        icon.width: 20
        icon.height: 16

        background: Rectangle {
            implicitWidth: 160
            implicitHeight: 34
            color: "transparent"

            radius: 14

            border.color: BSStyle.defaultBorderColor
            border.width: 1

        }

        onClicked: {
           fileDialog.open()
        }
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
            bsApp.exportWallet(wallet_properties_vm.walletId)
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
