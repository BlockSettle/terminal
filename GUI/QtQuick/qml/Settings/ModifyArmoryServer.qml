import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"

import terminal.models 1.0

ColumnLayout  {

    id: layout

    property var armoryServersModel: ({})
    property int server_index

    height: 548
    width: 580

    spacing: 0

    CustomTitleLabel {
        id: title
        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        Layout.preferredHeight : title.height
        text: qsTr("Modify custom server")
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

            // netType==0 => MainNet, netType==1 => TestNet
            onClicked : {
                armoryServersModel.setData(armoryServersModel.index(server_index, 0)
                                           , 0, ArmoryServersModel.NetTypeRole)
            }
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

            // netType==0 => MainNet, netType==1 => TestNet
            onClicked : {
                armoryServersModel.setData(armoryServersModel.index(server_index, 0)
                                           , 1, ArmoryServersModel.NetTypeRole)
            }
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

        onTextEdited: {
            armoryServersModel.setData(armoryServersModel.index(server_index, 0)
                                       , name_text_input.input_text, ArmoryServersModel.NameRole)
        }
    }

    CustomTextInput {
        id: ip_dns_text_input

        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        Layout.preferredHeight : 70
        Layout.preferredWidth: 532
        Layout.topMargin: 10

        title_text: qsTr("IP/DNS")

        input_validator: RegExpValidator { regExp: /(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)(\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)){3}(,(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)(\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)){3})*/ }

        onTextEdited: {
            armoryServersModel.setData(armoryServersModel.index(server_index, 0)
                                       , ip_dns_text_input.input_text, ArmoryServersModel.AddressRole)
        }
    }

    CustomTextInput {
        id: port_text_input

        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        Layout.preferredHeight : 70
        Layout.preferredWidth: 532
        Layout.topMargin: 10

        title_text: qsTr("Port")

        input_validator: IntValidator {bottom: 0; top: 65536;}

        onTextEdited: {
            armoryServersModel.setData(armoryServersModel.index(server_index, 0)
                                       , port_text_input.input_text, ArmoryServersModel.PortRole)
        }
    }

    CustomTextInput {
        id: db_key_text_input

        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        Layout.preferredHeight : 70
        Layout.preferredWidth: 532
        Layout.topMargin: 10

        title_text: qsTr("DB Key (optional)")

        onTextEdited: {
            armoryServersModel.setData(armoryServersModel.index(server_index, 0)
                                       , db_key_text_input.input_text, ArmoryServersModel.KeyRole)
        }
    }

    Label {
        id: spacer
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    function init()
    {
        // netType==0 => MainNet, netType==1 => TestNet
        var netType = armoryServersModel.data(armoryServersModel.index(server_index, 0), ArmoryServersModel.NetTypeRole)
        if (netType === 0)
        {
            radbut_main.checked = true
        }
        else if (netType === 1)
        {
            radbut_test.checked = true
        }
        name_text_input.input_text = armoryServersModel.data(armoryServersModel.index(server_index, 0)
                                                             , ArmoryServersModel.NameRole)
        ip_dns_text_input.input_text = armoryServersModel.data(armoryServersModel.index(server_index, 0)
                                                               , ArmoryServersModel.AddressRole)
        port_text_input.input_text = armoryServersModel.data(armoryServersModel.index(server_index, 0)
                                                               , ArmoryServersModel.PortRole)
        db_key_text_input.input_text = armoryServersModel.data(armoryServersModel.index(server_index, 0)
                                                               , ArmoryServersModel.KeyRole)
        name_text_input.setActiveFocus()
    }
}
