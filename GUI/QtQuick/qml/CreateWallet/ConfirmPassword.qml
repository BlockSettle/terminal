import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"

ColumnLayout  {

    id: layout

    property string wallet_name
    signal sig_confirm()

    height: BSSizes.applyScale(485)
    width: BSSizes.applyScale(580)

    spacing: 0

    CustomTitleLabel {
        id: title
        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : title.height
        text: qsTr("Set password")
    }

    CustomMessageDialog {
        id: error_dialog
        error: qsTr("Password strength is insufficient,\nplease use at least 6 characters")
        visible: false
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

        isPassword: true
        isHiddenText: true

        title_text: qsTr("Password")

        onEnterPressed: {
            click_enter()
        }
        onReturnPressed: {
            click_enter()
        }
        onTabNavigated: {
            if(checkPasswordLength()) {
                confirm_password.setActiveFocus()
            }
            else {
                password.setActiveFocus()
            }
        }
        onBackTabNavigated: {
            if(checkPasswordLength()) {
                if (confirm_but.enabled) {
                    confirm_but.setActiveFocus()
                }
                else {
                    confirm_password.setActiveFocus()
                }
            }
            else {
                password.setActiveFocus()
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
                    password.setActiveFocus()
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

        isPassword: true
        isHiddenText: true

        title_text: qsTr("Confirm Password")

        onEnterPressed: {
            confirm_but.click_enter()
        }
        onReturnPressed: {
            confirm_but.click_enter()
        }

        onTabNavigated: {
            if (confirm_but.enabled) {
                confirm_but.setActiveFocus()
            }
            else {
                password.setActiveFocus()
            }
        }
        onBackTabNavigated: {
            password.setActiveFocus()
        }
    }

    Label {
        id: spacer
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    CustomButton {
        id: confirm_but
        text: qsTr("Confirm")

        Layout.bottomMargin: BSSizes.applyScale(40)
        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

        width: BSSizes.applyScale(530)
        enabled: (password.input_text !== "")
                 && (confirm_password.input_text !== "")
                 && (password.input_text === confirm_password.input_text)
        preferred: true

        function click_enter() {
            if (!confirm_but.enabled) return

            if(password.input_text === confirm_password.input_text)
            {
                bsApp.createWallet(layout.wallet_name, phrase, password.input_text)
                layout.sig_confirm()
                clear()
            }
            else
            {
                password.isValid = false
                confirm_password.isValid = false
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
        confirm_password.isValid = true
        password.input_text = ""
        confirm_password.input_text = ""
    }

    function checkPasswordLength()
    {
        if (!bsApp.verifyPasswordIntegrity(password.input_text)) {
            error_dialog.show()
            error_dialog.raise()
            error_dialog.requestActivate()
            return false
        }
        return true
    }
}
