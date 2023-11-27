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

    CustomTitleLabel {
        id: title

        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : title.height

        text: qsTr("General")
    }

    CustomTextInput {
        id: log_file

        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : BSSizes.applyScale(70)
        Layout.preferredWidth: BSSizes.applyScale(532)
        Layout.topMargin: BSSizes.applyScale(24)

        input_topMargin: BSSizes.applyScale(35)
        title_leftMargin: BSSizes.applyScale(16)
        title_topMargin: BSSizes.applyScale(16)

        title_text: qsTr("Log file")

        onTextEdited : {
            bsApp.settingLogFile = log_file.input_text
        }
    }

    CustomTextInput {
        id: messages_log_file

        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : BSSizes.applyScale(70)
        Layout.preferredWidth: BSSizes.applyScale(532)
        Layout.topMargin: BSSizes.applyScale(10)

        input_topMargin: BSSizes.applyScale(35)
        title_leftMargin: BSSizes.applyScale(16)
        title_topMargin: BSSizes.applyScale(16)

        title_text: qsTr("Messages log file")

        onTextEdited : {
            bsApp.settingMsgLogFile = messages_log_file.input_text
        }
    }

    CustomCheckBox {
        id: checkbox_advanced_tx

        Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
        Layout.topMargin: BSSizes.applyScale(24)
        Layout.leftMargin: BSSizes.applyScale(24)

        text: qsTr("Advanced TX dialog by default")

        spacing: BSSizes.applyScale(6)
        font.pixelSize: BSSizes.applyScale(13)
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
