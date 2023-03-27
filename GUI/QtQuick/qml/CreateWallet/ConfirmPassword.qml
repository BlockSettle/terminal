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

    height: 485
    width: 580

    spacing: 0

    CustomTitleLabel {
        id: title
        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : title.height
        text: qsTr("Set password")
    }

    CustomMessageDialog {
        id: error_dialog
        error: qsTr("Password strength is insufficient")
        visible: false
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
        activeFocusOnTab: true

        isPassword: true
        isHiddenText: true

        title_text: qsTr("Password")

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

        isPassword: true
        isHiddenText: true

        title_text: qsTr("Confirm Password")
    }

    Label {
        id: spacer
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    CustomButton {
        id: confirm_but
        text: qsTr("Confirm")

        Layout.bottomMargin: 40
        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

        width: 530
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
        }
    }
}
