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
    signal sig_success()

    property var wallet_properties_vm
    property bool is_password_requried: !(wallet_properties_vm.isHardware || wallet_properties_vm.isWatchingOnly)

    Connections
    {
        target:bsApp
        function onSuccessDeleteWallet ()
        {
            layout.sig_success()
        }
        function onFailedDeleteWallet()
        {
            showError(qsTr("Failed to delete"))
        }
    }

    CustomMessageDialog {
        id: error_dialog
        error: qsTr("Password is incorrect")
        visible: false
    }

    CustomTitleLabel {
        id: title
        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : title.height
        text: qsTr("Delete wallet")
    }

    CustomTextInput {
        id: password
        visible: is_password_requried

        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : 70
        Layout.preferredWidth: 532
        Layout.topMargin: 10

        input_topMargin: 35
        title_leftMargin: 16
        title_topMargin: 16

        title_text: qsTr("Password")

        isPassword: true
        isHiddenText: true
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

            enabled: (password.input_text !== "" || !is_password_requried)

            width: 260

            function click_enter() {
                const result = bsApp.deleteWallet(
                    wallet_properties_vm.walletId,
                    password.input_text
                )

                if (result === -1) {
                    showError(qsTr("Failed to delete"))

                    init()
                }
            }
        }
    }

    function init()
    {
        clear()
        password.setActiveFocus()
    }

    function clear()
    {
        password.isValid = true
        password.input_text = ""
    }

    function showError(msg)
    {
        error_dialog.error = msg
        error_dialog.show()
        error_dialog.raise()
        error_dialog.requestActivate()
    }
}
