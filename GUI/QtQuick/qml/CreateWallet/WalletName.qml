import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"

ColumnLayout  {

    id: layout

    property alias wallet_name: input.input_text

    signal sig_confirm()

    height: 485
    width: 580

    spacing: 0

    CustomTitleLabel {
        id: title
        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : title.height
        text: qsTr("Name your wallet")
    }

    CustomMessageDialog {
        id: error_dialog
        error: qsTr("Wallet name already exist")
        visible: false
    }

    CustomTextInput {
        id: input

        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : 70
        Layout.preferredWidth: 532
        Layout.topMargin: 10

        input_topMargin: 35
        title_leftMargin: 16
        title_topMargin: 16
        activeFocusOnTab: true
        hide_placeholder_when_activefocus: false

        title_text: qsTr("Wallet Name")
        placeholder_text: qsTr("Primary wallet")

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
        text: qsTr("Confirm")

        Layout.bottomMargin: 40
        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

        width: 530
        enabled: (input.input_text !== "")
        preferred: true

        function click_enter() {
            if (!confirm_but.enabled) return

            if (bsApp.isWalletNameExist(input.input_text)) {
                error_dialog.show()
                error_dialog.raise()
                error_dialog.requestActivate()
                init()
            }
            else {
                layout.sig_confirm()
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
        input.setActiveFocus()
    }

    function clear()
    {
        input.input_text = ""
    }
}
