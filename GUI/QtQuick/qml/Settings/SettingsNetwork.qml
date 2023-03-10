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

//        Layout.preferredHeight: Math.min(armoryServersModel.rowCount * 50 + (armoryServersModel.rowCount - 1) * 10,
//                                         425)
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: 24
        Layout.topMargin: 24

        spacing: 10

        model: armoryServersModel

        ButtonGroup { id: radioGroup }

        delegate: CustomListRadioButton {
            id: _delegate

            title_text: name
            icon_add_source: "qrc:/images/delete.png"
            radio_checked: isCurrent
            radio_group: radioGroup

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
