import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"

ColumnLayout {
    id: layout

    height: 548
    width: 580

    spacing: 0

    signal back()
    signal walletDeleted()

    property var wallet_properties_vm

    CustomTitleLabel {
        id: title
        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : title.height
        text: qsTr("Delete wallet")
    }

    CustomTextInput {
        id: password

        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : 70
        Layout.preferredWidth: 532
        Layout.topMargin: 10

        input_topMargin: 35
        title_leftMargin: 16
        title_topMargin: 16

        title_text: qsTr("Password")

        Component.onCompleted: {
            password.isPassword = true
            password.isHiddenText = true
        }
    }

    Label {
        id: spacer
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    RowLayout {
        Layout.alignment: Qt.AlignHCenter

        CustomButton {
            text: qsTr("Back")

            Layout.bottomMargin: 40
            Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

            width: 260

            onClicked: back()
        }

        CustomButton {
            text: qsTr("Delete")
            preferred: true

            Layout.bottomMargin: 40
            Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

            enabled: (password.input_text !== "")

            width: 260


            function click_enter() {
                const result = bsApp.deleteWallet(
                    wallet_properties_vm.walletId,
                    password.input_text
                )

                if (result === 0) {
                    walletDeleted()
                    clear()
                }
            }
        }
    }

    function clear()
    {
        password.isValid = true
        password.input_text = ""
    }
}
