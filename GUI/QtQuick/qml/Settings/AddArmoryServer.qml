import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"

ColumnLayout  {

    id: layout

    property var armoryServersModel: ({})
    signal sig_added()

    height: 548
    width: 580

    spacing: 0

    CustomTitleLabel {
        id: title
        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        Layout.preferredHeight : title.height
        text: qsTr("Add custom server")
    }

    RowLayout {
        id: row
        spacing: 12

        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        Layout.topMargin: 24
        Layout.preferredHeight: 19

        Label {
            Layout.fillWidth: true
        }

        Label {
            id: radbut_text

            text: qsTr("Network type:")

            Layout.leftMargin: 25
            Layout.alignment: Qt.AlignVCenter

            width: 126
            height: 19

            color: "#E2E7FF"
            font.pixelSize: 16
            font.family: "Roboto"
            font.weight: Font.Normal
        }

        CustomRadioButton {
            id: radbut_main

            Layout.alignment: Qt.AlignVCenter

            text: "MainNet"

            spacing: 6
            font.pixelSize: 13
            font.family: "Roboto"
            font.weight: Font.Normal

            checked: true
        }

        CustomRadioButton {
            id: radbut_test

            Layout.alignment: Qt.AlignVCenter

            text: "TestNet"

            spacing: 6
            font.pixelSize: 13
            font.family: "Roboto"
            font.weight: Font.Normal

            checked: false
        }

        Label {
            Layout.fillWidth: true
        }
    }

    CustomTextInput {
        id: name_text_input

        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        Layout.preferredHeight : 70
        Layout.preferredWidth: 532
        Layout.topMargin: 10

        title_text: qsTr("Name")
    }

    CustomTextInput {
        id: ip_dns_text_input

        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        Layout.preferredHeight : 70
        Layout.preferredWidth: 532
        Layout.topMargin: 10

        title_text: qsTr("IP/DNS")

        input_validator: RegExpValidator { regExp: /(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)(\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)){3}(,(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)(\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)){3})*/ }
    }

    CustomTextInput {
        id: port_text_input

        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        Layout.preferredHeight : 70
        Layout.preferredWidth: 532
        Layout.topMargin: 10

        title_text: qsTr("Port")

        input_validator: IntValidator {bottom: 0; top: 65536;}
    }

    CustomTextInput {
        id: db_key_text_input

        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        Layout.preferredHeight : 70
        Layout.preferredWidth: 532
        Layout.topMargin: 10

        title_text: qsTr("DB Key (optional)")
    }

    CustomButton {
        id: save_but
        text: qsTr("Save")

        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        height : 70
        width: 532
        Layout.topMargin: 10

        enabled: (name_text_input.input_text !== "")
                 && (ip_dns_text_input.input_text !== "")
                 && (port_text_input.input_text !== "")
        preferred: true

        function click_enter() {
            if (!save_but.enabled) return

            var networkType = radbut_main.checked ? 0 : 1
            var name = radbut_main.checked ? name_text_input.input_text + " (Mainnet)" : name_text_input.input_text + " (Testnet)"
            var ip_dns = ip_dns_text_input.input_text
            var port = parseInt(port_text_input.input_text)
            var db_key = db_key_text_input.input_text

            armoryServersModel.add(name, ip_dns, port, networkType, db_key)

            clear()

            sig_added()
        }
    }

    Label {
        id: spacer
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    Keys.onEnterPressed: {
        save_but.click_enter()
    }

    Keys.onReturnPressed: {
        save_but.click_enter()
    }

    function init()
    {
        clear()
        name_text_input.setActiveFocus()
    }

    function clear()
    {
        ip_dns_text_input.input_text = ""
        port_text_input.input_text = ""
        db_key_text_input.input_text = ""
        name_text_input.input_text = ""
    }
}
