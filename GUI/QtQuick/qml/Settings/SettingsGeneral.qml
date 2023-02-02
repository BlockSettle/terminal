import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"

ColumnLayout  {

    id: layout

    signal sig_save()

    height: 548
    width: 580

    spacing: 0

    CustomTitleLabel {
        id: title

        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : title.height

        text: qsTr("General")
    }

    CustomTextInput {
        id: log_file

        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : 70
        Layout.preferredWidth: 532
        Layout.topMargin: 24

        input_topMargin: 35
        title_leftMargin: 16
        title_topMargin: 16

        title_text: qsTr("Log file")
    }

    CustomTextInput {
        id: messages_log_file

        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : 70
        Layout.preferredWidth: 532
        Layout.topMargin: 10

        input_topMargin: 35
        title_leftMargin: 16
        title_topMargin: 16

        title_text: qsTr("Messages log file")
    }

    CustomCheckBox {
        id: checkbox_advanced_tx

        Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
        Layout.topMargin: 24
        Layout.leftMargin: 24

        text: qsTr("Advanced TX dialog by default")

        spacing: 6
        font.pixelSize: 13
        font.family: "Roboto"
        font.weight: Font.Normal

        checked: true
    }

    Label {
        id: spacer
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    CustomButton {
        id: save_but

        Layout.bottomMargin: 40
        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

        enabled: log_file.input_text.length && messages_log_file.input_text.length

        width: 532

        text: qsTr("Save")

        Component.onCompleted: {
            save_but.preferred = true
        }

        function click_enter() {
            if (!save_but.enabled)
                return

            bsApp.settingLogFile = log_file.input_text
            bsApp.settingMsgLogFile = messages_log_file.input_text
            bsApp.settingAdvancedTX = checkbox_advanced_tx.checked

            layout.sig_save()
        }
    }

    Keys.onEnterPressed: {
        save_but.click_enter()
    }

    Keys.onReturnPressed: {
        save_but.click_enter()
    }

    function init()
    {
        messages_log_file.input_text = bsApp.settingMsgLogFile
        checkbox_advanced_tx.checked = bsApp.settingAdvancedTX
        log_file.input_text = bsApp.settingLogFile

        log_file.setActiveFocus()
    }
}
