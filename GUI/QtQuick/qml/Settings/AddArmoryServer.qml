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

    height: BSSizes.applyScale(548)
    width: BSSizes.applyScale(580)

    spacing: 0

    CustomTitleLabel {
        id: title
        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        Layout.preferredHeight : title.height
        text: qsTr("Add custom server")
    }

    RowLayout {
        id: row
        spacing: BSSizes.applyScale(12)

        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        Layout.topMargin: BSSizes.applyScale(24)
        Layout.preferredHeight: BSSizes.applyScale(19)

        Label {
            Layout.fillWidth: true
        }

        Label {
            id: radbut_text

            text: qsTr("Network type:")

            Layout.leftMargin: BSSizes.applyScale(25)
            Layout.alignment: Qt.AlignVCenter

            width: BSSizes.applyScale(126)
            height: BSSizes.applyScale(19)

            color: "#E2E7FF"
            font.pixelSize: BSSizes.applyScale(16)
            font.family: "Roboto"
            font.weight: Font.Normal
        }

        CustomRadioButton {
            id: radbut_main

            Layout.alignment: Qt.AlignVCenter

            text: "MainNet"

            spacing: BSSizes.applyScale(6)
            font.pixelSize: BSSizes.applyScale(13)
            font.family: "Roboto"
            font.weight: Font.Normal

            checked: true
        }

        CustomRadioButton {
            id: radbut_test

            Layout.alignment: Qt.AlignVCenter

            text: "TestNet"

            spacing: BSSizes.applyScale(6)
            font.pixelSize: BSSizes.applyScale(13)
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
        Layout.preferredHeight : BSSizes.applyScale(70)
        Layout.preferredWidth: BSSizes.applyScale(532)
        Layout.topMargin: BSSizes.applyScale(10)

        title_text: qsTr("Name")

        onTabNavigated: ip_dns_text_input.setActiveFocus()
    }

    CustomTextInput {
        id: ip_dns_text_input

        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        Layout.preferredHeight : BSSizes.applyScale(70)
        Layout.preferredWidth: BSSizes.applyScale(532)
        Layout.topMargin: BSSizes.applyScale(10)

        title_text: qsTr("IP/DNS")

        onTabNavigated: port_text_input.setActiveFocus()
    }

    CustomTextInput {
        id: port_text_input

        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        Layout.preferredHeight : BSSizes.applyScale(70)
        Layout.preferredWidth: BSSizes.applyScale(532)
        Layout.topMargin: BSSizes.applyScale(10)

        title_text: qsTr("Port")

        input_validator: IntValidator {bottom: 80; top: 65535;}

        onTabNavigated: db_key_text_input.setActiveFocus()
    }

    CustomTextInput {
        id: db_key_text_input

        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        Layout.preferredHeight : BSSizes.applyScale(70)
        Layout.preferredWidth: BSSizes.applyScale(532)
        Layout.topMargin: BSSizes.applyScale(10)

        title_text: qsTr("DB Key (optional)")

        onTabNavigated: save_but.forceActiveFocus()
    }

    CustomButton {
        id: save_but
        text: qsTr("Save")

        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        height : BSSizes.applyScale(70)
        width: BSSizes.applyScale(532)
        Layout.topMargin: BSSizes.applyScale(10)

        enabled: (name_text_input.input_text !== "")
                 && (ip_dns_text_input.input_text !== "")
                 && (port_text_input.input_text !== "")
        preferred: true

        function click_enter() {
            if (!save_but.enabled) return

            var networkType = radbut_main.checked ? 0 : 1
            var name = name_text_input.input_text
            var ip_dns = ip_dns_text_input.input_text
            var port = parseInt(port_text_input.input_text)
            var db_key = db_key_text_input.input_text

            bsApp.addArmoryServer(armoryServersModel, name, networkType, ip_dns, port, db_key)

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
