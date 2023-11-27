import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15
import Qt.labs.platform 1.1 as QLP

import "../BsStyles"
import "../StyledControls"

ColumnLayout  {

    id: layout

    signal sig_continue()

    property var phrase

    height: BSSizes.applyScale(485)
    width: BSSizes.applyScale(580)
    spacing: 0

    CustomTitleLabel {
        id: title
        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : title.height
        text: qsTr("Create new wallet")
    }

    Label {
        id: subtitle
        Layout.alignment: Qt.AlignCenter
        Layout.topMargin: BSSizes.applyScale(16)
        Layout.preferredHeight : BSSizes.applyScale(16)
        text: qsTr("Write down and store your 12 word seed someplace safe and offline")
        color: "#E2E7FF"
        font.pixelSize: BSSizes.applyScale(14)
        font.family: "Roboto"
        font.weight: Font.Normal
    }

    GridView {
        id: grid

        Layout.fillHeight: true
        Layout.fillWidth: true
        Layout.leftMargin: BSSizes.applyScale(25)
        Layout.topMargin: BSSizes.applyScale(32)

        cellHeight : BSSizes.applyScale(56)
        cellWidth : BSSizes.applyScale(180)

        model: phrase
        delegate: CustomSeedLabel {
            seed_text: modelData
            serial_num: index + 1
        }
    }

    RowLayout {
        id: row
        spacing: BSSizes.applyScale(10)

        //Layout.leftMargin: 24
        Layout.bottomMargin: BSSizes.applyScale(40)
        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter
        

        QLP.FileDialog {
            id: exportFileDialog  
            title: qsTr("Please choose folder to export pdf")
            folder: QLP.StandardPaths.writableLocation(QLP.StandardPaths.DocumentsLocation)
            defaultSuffix: "pdf"
            fileMode: QLP.FileDialog.SaveFile
            onAccepted: {
                bsApp.exportWalletToPdf(exportFileDialog.currentFile, phrase)
                Qt.openUrlExternally(exportFileDialog.currentFile);
            }
        }

        CustomButton {
            id: copy_seed_but
            text: qsTr("Export PDF")
            width: BSSizes.applyScale(261)

            preferred: false

            function click_enter() {
                exportFileDialog.currentFile = "file:///" + bsApp.makeExportWalletToPdfPath(phrase)
                exportFileDialog.open()
            }
        }

        CustomButton {
            id: continue_but
            text: qsTr("Continue")
            width: BSSizes.applyScale(261)

            preferred: true

            function click_enter() {
                layout.sig_continue()
            }
        }
    }

    function init()
    {
        continue_but.forceActiveFocus()
    }
}
