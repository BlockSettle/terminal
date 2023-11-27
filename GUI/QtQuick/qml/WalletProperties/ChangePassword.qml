import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"

ColumnLayout {
    id: layout

    signal sig_success()

    height: BSSizes.applyScale(548)
    width: BSSizes.applyScale(580)

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
        error: qsTr("Password strength is insufficient,\nplease use at least 6 characters")
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
        Layout.preferredHeight : BSSizes.applyScale(70)
        Layout.preferredWidth: BSSizes.applyScale(532)
        Layout.topMargin: BSSizes.applyScale(10)

        input_topMargin: BSSizes.applyScale(35)
        title_leftMargin: BSSizes.applyScale(16)
        title_topMargin: BSSizes.applyScale(16)
        activeFocusOnTab: true

        title_text: qsTr("Password")

        isPassword: true
        isHiddenText: true

        onEnterPressed: {
            click_enter()
        }
        
        onReturnPressed: {
            click_enter()
        }

        onTabNavigated: {
            new_password.setActiveFocus()
        }
        onBackTabNavigated: {
            if (confirm_but.enabled){
                confirm_but.setActiveFocus()
            }
            else {
                confirm_password.setActiveFocus()
            }
        }

        function click_enter() {
            if (confirm_but.enabled) {
                confirm_but.click_enter()
            } 
            else {
                new_password.setActiveFocus()
            }
        }
    }

    CustomTextInput {
        id: new_password

        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : BSSizes.applyScale(70)
        Layout.preferredWidth: BSSizes.applyScale(532)
        Layout.topMargin: BSSizes.applyScale(10)

        input_topMargin: BSSizes.applyScale(35)
        title_leftMargin: BSSizes.applyScale(16)
        title_topMargin: BSSizes.applyScale(16)
        activeFocusOnTab: true

        title_text: qsTr("New Password")

        isPassword: true
        isHiddenText: true

        onEnterPressed: {
            click_enter()
        }
        
        onReturnPressed: {
            click_enter()
        }

        onTabNavigated: {
            if (checkPasswordLength()) {
                confirm_password.setActiveFocus()
            }
            else {
                new_password.setActiveFocus()
            }
        }
        onBackTabNavigated: {
            if (checkPasswordLength()) {
                password.setActiveFocus()
            }
            else {
                new_password.setActiveFocus()
            }
        }

        function click_enter() {
            if (confirm_but.enabled) {
                confirm_but.click_enter()
            }
            else {
                if (checkPasswordLength()) {
                    confirm_password.setActiveFocus()
                }
                else {
                    new_password.setActiveFocus()
                }
            }
        }
    }

    CustomTextInput {
        id: confirm_password

        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : BSSizes.applyScale(70)
        Layout.preferredWidth: BSSizes.applyScale(532)
        Layout.topMargin: BSSizes.applyScale(10)

        input_topMargin: BSSizes.applyScale(35)
        title_leftMargin: BSSizes.applyScale(16)
        title_topMargin: BSSizes.applyScale(16)

        title_text: qsTr("Confirm Password")

        isPassword: true
        isHiddenText: true

        onEnterPressed: {
            click_enter()
        }
        
        onReturnPressed: {
            click_enter()
        }

        onTabNavigated: {
            if (confirm_but.enabled) {
                confirm_but.setActiveFocus()
            }
            else {
                password.setActiveFocus()
            }
        }
        onBackTabNavigated: new_password.setActiveFocus()

        function click_enter() {
            if (confirm_but.enabled) {
                confirm_but.click_enter()
            }
        }
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

        Layout.bottomMargin: BSSizes.applyScale(40)
        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

        width: BSSizes.applyScale(530)
        enabled: (password.input_text !== "")
                 && (new_password.input_text !== "")
                 && (confirm_password.input_text !== "")
                 && (new_password.input_text === confirm_password.input_text)

        function click_enter() {
            if (!confirm_but.enabled) {
                return
            }
            
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

    // Keys.onEnterPressed: {
    //     confirm_but.click_enter()
    // }

    // Keys.onReturnPressed: {
    //     confirm_but.click_enter()
    // }

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
        if (!visible) {
            return false
        }
        
        if (!bsApp.verifyPasswordIntegrity(new_password.input_text)) {
            error_dialog.show()
            error_dialog.raise()
            error_dialog.requestActivate()
            return false
        }
        return true
    }
}
