import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"

ColumnLayout  {

    id: layout

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

        onTextEdited : {
            bsApp.settingLogFile = log_file.input_text
        }
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

        onTextEdited : {
            bsApp.settingMsgLogFile = messages_log_file.input_text
        }
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

        onClicked: {
            bsApp.settingAdvancedTX = checkbox_advanced_tx.checked
        }
    }

    Label {
        id: spacer
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    function init()
    {
        messages_log_file.input_text = bsApp.settingMsgLogFile
        checkbox_advanced_tx.checked = bsApp.settingAdvancedTX
        log_file.input_text = bsApp.settingLogFile

        //log_file.setActiveFocus()
    }
}
