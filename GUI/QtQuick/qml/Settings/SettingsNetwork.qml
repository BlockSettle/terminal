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

        text: qsTr("Network")
    }

    CustomTextInput {
        id: host

        input_validator: RegExpValidator { regExp: /(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)(\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)){3}(,(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)(\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)){3})*/ }

        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : 70
        Layout.preferredWidth: 532
        Layout.topMargin: 24

        input_topMargin: 35
        title_leftMargin: 16
        title_topMargin: 16

        title_text: qsTr("Armory host")

        onTextEdited : {
            bsApp.settingArmoryHost = host.input_text
        }
    }

    CustomTextInput {
        id: port

        input_validator: IntValidator {bottom: 0; top: 65536;}

        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : 70
        Layout.preferredWidth: 532
        Layout.topMargin: 10

        input_topMargin: 35
        title_leftMargin: 16
        title_topMargin: 16

        title_text: qsTr("Armory port")

        onTextEdited : {
            bsApp.settingArmoryPort = port.input_text
        }
    }

    Label {
        id: spacer
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    function init()
    {
        port.input_text = bsApp.settingArmoryPort
        host.input_text = bsApp.settingArmoryHost

        host.setActiveFocus()
    }
}
