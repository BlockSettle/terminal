import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "StyledControls"
import "BsStyles"

ColumnLayout  {

    id: layout

    signal sig_confirm()

    height: 481
    width: 580
    implicitHeight: 481
    implicitWidth: 580

    spacing: 0

    CustomTitleLabel {
        id: title
        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : title.height
        text: qsTr("Set password")
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

        Component.onCompleted: {
            confirm_password.isPassword = true
            confirm_password.isHiddenText = true
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
        Layout.leftMargin: 25
        Layout.bottomMargin: 40
        width: 530
        enabled: (wallet_name.input_text !== "")
                 && (password.input_text !== "")
                 && (confirm_password.input_text !== "")

        Component.onCompleted: {
            confirm_but.preferred = true
        }
        onClicked: {
            if(password.input_text === confirm_password.input_text)
            {
                bsApp.createWallet("", phrase, password.input_text)
                layout.sig_confirm()
                password.isValid = true
                confirm_password.isValid = true
                password.input_text = ""
                confirm_password.input_text = ""
            }
            else
            {
                password.isValid = false
                confirm_password.isValid = false
            }
        }
    }
}
