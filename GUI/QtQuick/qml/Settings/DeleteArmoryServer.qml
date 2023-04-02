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

    signal sig_back()
    signal sig_delete()

    height: 548
    width: 580
    spacing: 0

    CustomTitleLabel {
        id: title
        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        Layout.preferredHeight : title.height
        text: qsTr("Delete custom server")
    }

    Image {
        id: wallet_icon

        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        Layout.topMargin: 24
        Layout.preferredHeight : 120
        Layout.preferredWidth : 120

        source: "qrc:/images/delete_custom_server.svg"
        width: 120
        height: 120
    }

    Label {
        id: description

        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        Layout.topMargin: 16
        Layout.preferredHeight : 16

        text: qsTr("Are you sure you want to delete the \"%1\" server?")
        .arg(armoryServersModel.data(armoryServersModel.index(server_index, 0), ArmoryServersModel.NameRole))

        color: BSStyle.titanWhiteColor
        font.pixelSize: 14
        font.family: "Roboto"
        font.weight: Font.Normal
    }

    Label {
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    RowLayout {
        id: row
        spacing: 11

        Layout.bottomMargin: 40
        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

        CustomButton {
            id: back_but
            text: qsTr("No, back")
            width: 262

            preferred: false
            function click_enter() {
                sig_back()
            }
        }

        CustomButton {
            id: delete_but
            text: qsTr("Yes, delete")
            width: 262

            preferred: true
            function click_enter() {
                bsApp.delArmoryServer(armoryServersModel, server_index)
                sig_delete()
            }
        }
    }

    function init()
    {
        delete_but.forceActiveFocus()
    }
}
