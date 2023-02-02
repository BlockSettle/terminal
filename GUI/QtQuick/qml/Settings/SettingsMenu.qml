import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"

ColumnLayout  {

    id: layout

    signal sig_general()
    signal sig_network()
    signal sig_about()

    height: 548
    width: 580

    spacing: 0

    CustomTitleLabel {
        id: title

        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : title.height

        text: qsTr("Settings")
    }

    CustomListItem {
        id: general_item

        Layout.alignment: Qt.AlignCenter
        Layout.topMargin: 24

        //properties
        isButton: true

        //aliases
        icon_source: "qrc:/images/general.png"
        icon_add_source: "qrc:/images/arrow.png"
        tytle_text: "General"

        onClicked: sig_general()
    }

    CustomListItem {
        id: network_item

        Layout.alignment: Qt.AlignCenter
        Layout.topMargin: 10

        //properties
        isButton: true

        //aliases
        icon_source: "qrc:/images/network.png"
        icon_add_source: "qrc:/images/arrow.png"
        tytle_text: "Network"

        onClicked: sig_network()
    }

    CustomListItem {
        id: about_item

        Layout.alignment: Qt.AlignCenter
        Layout.topMargin: 10

        //properties
        isButton: true

        //aliases
        icon_source: "qrc:/images/about.png"
        icon_add_source: "qrc:/images/arrow.png"
        tytle_text: "About"

        onClicked: sig_about()
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
