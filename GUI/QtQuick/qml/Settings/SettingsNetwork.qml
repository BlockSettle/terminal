import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"

ColumnLayout  {

    id: layout

    property var armoryServersModel: ({})

    signal sig_add_custom()

    height: 548
    width: 580

    spacing: 0

    CustomTitleLabel {
        id: title

        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : title.height

        text: qsTr("Network")
    }

    ListView {
        id: list

        Layout.fillHeight: true
        Layout.fillWidth: true
        Layout.leftMargin: 25
        Layout.topMargin: 32

        spacing: 10

        model: armoryServersModel

        delegate: CustomListRadioButton {
            id: _delegate

            width: 530
            title_text: name
            icon_add_source: "qrc:/images/delete.png"
            radio_checked: isCurrent

            onClicked_add: {
                if (!isDefault)
                {
                    bsApp.delArmoryServer(armoryServersModel, index)
                }
            }

            onSig_radio_clicked: {
                if (radio_checked)
                {
                    isCurrent = true
                }
            }
        }

    }

    CustomListItem {
        id: add_custom_server

        Layout.alignment: Qt.AlignCenter
        Layout.topMargin: 10

        //aliases
        icon_source: "qrc:/images/plus.svg"
        title_text: qsTr("Add custom server")

        onClicked: sig_add_custom()
    }

    Label {
        id: spacer
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    function init()
    {
    }

    Component.onCompleted: {
        layout.armoryServersModel = bsApp.getArmoryServers()
    }
}
