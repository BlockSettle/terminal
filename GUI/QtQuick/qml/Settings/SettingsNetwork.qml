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
    signal sig_delete_custom(int ind)
    signal sig_modify_custom(int ind)
    signal request_close()

    height: BSSizes.applyScale(548)
    width: BSSizes.applyScale(580)

    spacing: 0

    CustomTitleLabel {
        id: title

        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : title.height

        text: qsTr("Network")
    }

    ListView {
        id: list

        Layout.fillWidth: true
        Layout.minimumHeight: BSSizes.applyScale(50) * armoryServersModel.rowCount + BSSizes.applyScale(10) * (armoryServersModel.rowCount - 1)
        Layout.leftMargin: BSSizes.applyScale(24)
        Layout.topMargin: BSSizes.applyScale(24)

        spacing: BSSizes.applyScale(10)

        clip: true
        boundsBehavior: Flickable.StopAtBounds

        flickDeceleration: 750
        maximumFlickVelocity: 1000

        ScrollBar.vertical: ScrollBar {
            id: verticalScrollBar
            policy: list.contentHeight > list.height ? ScrollBar.AlwaysOn : ScrollBar.AsNeeded
        }

        model: armoryServersModel

        ButtonGroup { id: radioGroup }

        delegate: CustomListRadioButton {
            id: _delegate

            title_text: display
            icon_add_source: isDefault ? "" : "qrc:/images/delete.png"
            radio_checked: isCurrent
            radio_group: radioGroup
            icon_add_z: 1

            onClicked_add: {
                if (!isDefault)
                {
                    sig_delete_custom (index)
                }
            }

            onClicked: {
                if (isCurrent === false)
                {
                    isCurrent = true
                    request_close()
                }
                else if (isDefault === false)
                {
                    sig_modify_custom (index)
                }
            }
        }

        Connections
        {
            target:armoryServersModel
            function onRowCountChanged ()
            {
                var new_height = Math.min(armoryServersModel.rowCount * 50 + (armoryServersModel.rowCount - 1) * 10, 425)
                list.implicitHeight = new_height
            }
        }
    }

    CustomListItem {
        id: add_custom_server

        Layout.alignment: Qt.AlignCenter
        Layout.topMargin: BSSizes.applyScale(10)
        Layout.leftMargin: BSSizes.applyScale(24)
        Layout.rightMargin: BSSizes.applyScale(24)
        Layout.fillWidth: true

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
}
