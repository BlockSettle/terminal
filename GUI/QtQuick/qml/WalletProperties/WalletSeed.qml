import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"

ColumnLayout  {
    id: layout

    height: BSSizes.applyScale(548)
    width: BSSizes.applyScale(580)

    spacing: 0

    property var wallet_properties_vm

    CustomTitleLabel {
        id: title
        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : title.height
        text: qsTr("Wallet seed")
    }

    GridView {
        id: grid

        Layout.fillHeight: true
        Layout.fillWidth: true
        Layout.leftMargin: BSSizes.applyScale(25)
        Layout.topMargin: BSSizes.applyScale(32)

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AlwaysOn
        }

        cellHeight : BSSizes.applyScale(56)
        cellWidth : BSSizes.applyScale(180)

        model: wallet_properties_vm.seed
        delegate: CustomSeedLabel {
            seed_text: modelData
            serial_num: index + 1
        }
    }

    CustomButton {
        id: confirm_but
        text: qsTr("Print PDF")
        preferred: true

        Layout.bottomMargin: BSSizes.applyScale(40)
        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

        width: BSSizes.applyScale(530)

        onClicked: {
            var printedPath = bsApp.exportPRK()
            successPrintPdf.details_text = qsTr("PDF successfully saved to ") + printedPath
            
            successPrintPdf.show()
            successPrintPdf.raise()
            successPrintPdf.requestActivate()
        }
    }

    CustomSuccessDialog {
        id: successPrintPdf
        visible: false
    }
}
