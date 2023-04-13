import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"

ColumnLayout {
    id: layout

    height: BSSizes.applyScale(548)
    width: BSSizes.applyScale(580)

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
            if (!layout.visible) {
                return
            }
            
            layout.sig_success()
        }
        function onFailedDeleteWallet()
        {
            if (!layout.visible) {
                return
            }
            
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
        Layout.preferredHeight : BSSizes.applyScale(70)
        Layout.preferredWidth: BSSizes.applyScale(532)
        Layout.topMargin: BSSizes.applyScale(10)

        input_topMargin: BSSizes.applyScale(35)
        title_leftMargin: BSSizes.applyScale(16)
        title_topMargin: BSSizes.applyScale(16)

        title_text: qsTr("Password")

        isPassword: true
        isHiddenText: true

        onEnterPressed: {
            delete_btn.click_enter()
        }
        onReturnPressed: {
            delete_btn.click_enter()
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

            Layout.bottomMargin: BSSizes.applyScale(40)
            Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

            width: BSSizes.applyScale(260)

            onClicked: back()
        }

        CustomButton {
            id: delete_btn
            text: qsTr("Delete")
            preferred: true

            Layout.bottomMargin: BSSizes.applyScale(40)
            Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

            enabled: (password.input_text !== "" || !is_password_requried)

            width: BSSizes.applyScale(260)

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
