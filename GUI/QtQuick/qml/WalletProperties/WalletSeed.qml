import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"

ColumnLayout  {
    id: layout

    height: 548
    width: 580

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
        Layout.leftMargin: 25
        Layout.topMargin: 32

        cellHeight : 56
        cellWidth : 180

        model: wallet_properties_vm.seed
        delegate: CustomSeedLabel {
            seed_text: modelData
            serial_num: index + 1
        }
    }

    Label {
        id: spacer
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    CustomButton {
        id: confirm_but
        text: qsTr("Copy seed")
        preferred: true

        Layout.bottomMargin: 40
        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

        width: 530

        onClicked: bsApp.copySeedToClipboard(
            wallet_properties_vm.seed
        )
    }
}
