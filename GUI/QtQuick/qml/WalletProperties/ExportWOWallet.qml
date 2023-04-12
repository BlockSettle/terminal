import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15
import QtQuick.Dialogs 1.0

import "../BsStyles"
import "../StyledControls"

ColumnLayout  {
    id: layout

    signal sig_success(string nameExport, string pathExport)

    property var wallet_properties_vm
    property bool isExitWhenSuccess

    height: BSSizes.applyScale(548)
    width: BSSizes.applyScale(580)
    focus: true

    spacing: 0


    Connections
    {
        target:bsApp
        function onSuccessExport (nameExport)
        {
            layout.sig_success(nameExport, bsApp.settingExportDir)
        }
    }

    CustomTitleLabel {
        id: title
        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : title.height
        text: qsTr("Export watching-only wallet")
    }

    Rectangle {
        width: BSSizes.applyScale(530)
        height: BSSizes.applyScale(82)
        radius: BSSizes.applyScale(14)
        color: BSStyle.exportWalletLabelBackground

        Layout.topMargin: BSSizes.applyScale(24)
        Layout.alignment: Qt.AlignCenter

        Grid {
            columns: 2
            rowSpacing: BSSizes.applyScale(14)
            width: parent.width
            anchors.centerIn: parent

            Text {
                text: qsTr("Wallet name")
                color: BSStyle.exportWalletLabelNameColor
                font.family: "Roboto"
                font.pixelSize: BSSizes.applyScale(14)
                width: parent.width / 2
                leftPadding: BSSizes.applyScale(20)
            }
            Text {
                text: wallet_properties_vm.walletName
                color: BSStyle.textColor
                font.family: "Roboto"
                font.pixelSize: BSSizes.applyScale(14)
                width: parent.width / 2
                horizontalAlignment: Text.AlignRight
                rightPadding: BSSizes.applyScale(20)
            }

            Text {
                text: qsTr("Wallet ID")
                color: BSStyle.exportWalletLabelNameColor
                font.family: "Roboto"
                font.pixelSize: BSSizes.applyScale(14)
                width: parent.width / 2
                leftPadding: BSSizes.applyScale(20)
            }
            Text {
                text: wallet_properties_vm.walletId
                color: BSStyle.textColor
                font.family: "Roboto"
                font.pixelSize: BSSizes.applyScale(14)
                width: parent.width / 2
                horizontalAlignment: Text.AlignRight
                rightPadding: BSSizes.applyScale(20)
            }
        }
    }

    Label {

        Layout.leftMargin: BSSizes.applyScale(24)
        Layout.rightMargin: BSSizes.applyScale(24)
        Layout.topMargin: BSSizes.applyScale(32)
        Layout.preferredHeight: BSSizes.applyScale(160)
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignLeft | Qt.AlingTop

        font.pixelSize: BSSizes.applyScale(14)
        font.family: "Roboto"
        font.weight: Font.Normal

        text: qsTr("Backup file:")
        color: BSStyle.wildBlueColor
    }

    Label {

        Layout.leftMargin: BSSizes.applyScale(24)
        Layout.rightMargin: BSSizes.applyScale(24)
        Layout.topMargin: BSSizes.applyScale(8)
        Layout.preferredHeight: BSSizes.applyScale(16)
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignLeft | Qt.AlingTop

        font.pixelSize: BSSizes.applyScale(14)
        font.family: "Roboto"
        font.weight: Font.Normal

        text: bsApp.settingExportDir
        color: BSStyle.titanWhiteColor
    }

    Button {

        Layout.leftMargin: BSSizes.applyScale(24)
        Layout.topMargin: BSSizes.applyScale(12)
        Layout.alignment: Qt.AlignLeft | Qt.AlingTop

        activeFocusOnTab: false

        font.pixelSize: BSSizes.applyScale(13)
        font.family: "Roboto"
        font.weight: Font.Normal
        palette.buttonText: BSStyle.buttonsHeaderTextColor

        text: qsTr("Select target dir")

        icon.color: BSStyle.wildBlueColor
        icon.source: "qrc:/images/folder_icon.png"
        icon.width: BSSizes.applyScale(20)
        icon.height: BSSizes.applyScale(16)

        background: Rectangle {
            implicitWidth: BSSizes.applyScale(160)
            implicitHeight: BSSizes.applyScale(34)
            color: "transparent"

            radius: BSSizes.applyScale(14)

            border.color: BSStyle.defaultBorderColor
            border.width: BSSizes.applyScale(1)

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

        enabled: bsApp.settingExportDir.length !== 0
        preferred: true
        focus: true
        text: qsTr("Export")

        Layout.bottomMargin: BSSizes.applyScale(40)
        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

        width: BSSizes.applyScale(530)

        function click_enter() {
            bsApp.exportWallet(wallet_properties_vm.walletId, bsApp.settingExportDir)
            layout.sig_export()
        }
    }

    Keys.onEnterPressed: {
        confirm_but.click_enter()
    }

    Keys.onReturnPressed: {
        confirm_but.click_enter()
    }

    FileDialog {
        id: fileDialog
        title: qsTr("Please choose a directory")
        folder: shortcuts.documents
        selectFolder: true

        onAccepted: {
            var res = fileDialog.fileUrl.toString().replace(/^(file:\/{3})/,"")
            if (res)
            {
                bsApp.settingExportDir = res
            }
        }
    }

    function init() {
        confirm_but.setActiveFocus()
    }
}
