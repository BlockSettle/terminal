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

    signal authorized()

    Connections {
        target: bsApp
        function onWalletSeedAuthFailed(error) {
            init()
            show_error(error)
        }
        function onWalletSeedAuthSuccess() {
            authorized()
            clear()
        }
    }

    CustomFailDialog {
        id: fail_dialog
        header: qsTr("Error")
        visible: false
    }

    CustomTitleLabel {
        id: title
        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : title.height
        text: qsTr("View wallet seed")
    }

    CustomTextInput {
        id: password

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
            confirm_but.click_enter()
        }
        onReturnPressed: {
            confirm_but.click_enter()
        }
    }

    Label {
        id: spacer
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    CustomButton {
        id: confirm_but
        text: qsTr("Continue")
        preferred: true

        Layout.bottomMargin: BSSizes.applyScale(40)
        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

        width: BSSizes.applyScale(530)
        enabled: (password.input_text !== "")

        function click_enter() {
            if (!confirm_but.enabled) {
                return
            }

            const result = bsApp.viewWalletSeedAuth(
                wallet_properties_vm.walletId,
                password.input_text
            )
            if (result !== 0) {
                show_error(qsTr("Failed to send wallet seed message"))
                clear()
            }
        }
    }

    Keys.onEnterPressed: {
        confirm_but.click_enter()
    }

    Keys.onReturnPressed: {
        confirm_but.click_enter()
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

    function show_error(error)
    {
        fail_dialog.fail = error
        fail_dialog.show()
        fail_dialog.raise()
        fail_dialog.requestActivate()
    }
}
