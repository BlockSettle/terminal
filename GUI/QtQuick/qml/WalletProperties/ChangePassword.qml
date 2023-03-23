import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"

ColumnLayout {
    id: layout

    signal sig_success()

    height: 548
    width: 580

    spacing: 0

    property var wallet_properties_vm

    Connections
    {
        target:bsApp
        function onSuccessChangePassword ()
        {
            layout.sig_success()
        }
    }

    CustomMessageDialog {
        id: error_dialog
        error: qsTr("Password should be over 6 charaters")
        visible: false
    }

    CustomTitleLabel {
        id: title
        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : title.height
        text: qsTr("Change password")
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

        isPassword: true
        isHiddenText: true
    }

    CustomTextInput {
        id: new_password

        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : 70
        Layout.preferredWidth: 532
        Layout.topMargin: 10

        input_topMargin: 35
        title_leftMargin: 16
        title_topMargin: 16
        activeFocusOnTab: true

        title_text: qsTr("New Password")

        isPassword: true
        isHiddenText: true

        Keys.onEnterPressed: {
            checkPasswordLength()
            confirm_password.setActiveFocus()
        }
        Keys.onReturnPressed: {
            checkPasswordLength()
            confirm_password.setActiveFocus()
        }
        onTabNavigated: {
            checkPasswordLength()
            confirm_password.setActiveFocus()
        }
        onBackTabNavigated: {
            checkPasswordLength()
            password.setActiveFocus()
        }
    }

    CustomTextInput {
        id: confirm_password

        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : 70
        Layout.preferredWidth: 532
        Layout.topMargin: 10

        input_topMargin: 35
        title_leftMargin: 16
        title_topMargin: 16

        title_text: qsTr("Confirm Password")

        isPassword: true
        isHiddenText: true
    }

    Label {
        id: spacer
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    CustomButton {
        id: confirm_but
        text: qsTr("Save")
        preferred: true

        Layout.bottomMargin: 40
        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

        width: 530
        enabled: (password.input_text !== "")
                 && (new_password.input_text !== "")
                 && (confirm_password.input_text !== "")
                 && (new_password.input_text === confirm_password.input_text)

        function click_enter() {
            const result = bsApp.changePassword(
                wallet_properties_vm.walletId,
                password.input_text,
                new_password.input_text,
                confirm_password.input_text
            )
            if (result === 0) {
                init()
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
        new_password.isValid = true
        confirm_password.isValid = true
        password.input_text = ""
        new_password.input_text = ""
        confirm_password.input_text = ""
    }

    function checkPasswordLength()
    {
        if (new_password.input_text.length < 6) {
            error_dialog.show()
            error_dialog.raise()
            error_dialog.requestActivate()
        }
    }
}
