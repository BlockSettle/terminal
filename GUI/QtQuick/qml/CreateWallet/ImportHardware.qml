import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"

ColumnLayout  {

    id: layout

    property bool isConnected: false

    signal sig_import()

    height: 481
    width: 580
    implicitHeight: 481
    implicitWidth: 580

    spacing: 0

    CustomTitleLabel {
        id: title
        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : title.height
        text: qsTr("Import hardware")
    }

    Image {
        id: usb_icon

        Layout.alignment: Qt.AlignCenter
        Layout.topMargin: 24
        Layout.preferredHeight : 44
        Layout.preferredWidth : 44

        width: 44
        height: 44

        source: layout.isConnected ? "qrc:/images/USB_icon_conn.png" : "qrc:/images/USB_icon_disconn.png"
    }


    Label {
        id: subtitle
        Layout.alignment: Qt.AlignCenter
        Layout.topMargin: 16
        Layout.preferredHeight : 16
        text: qsTr("Connect your wallet")
        color: "#E2E7FF"
        font.pixelSize: 14
        font.family: "Roboto"
        font.weight: Font.Normal
    }

    Label {
        id: spacer
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    CustomButton {
        id: confirm_but
        text: qsTr("Import")

        //Layout.leftMargin: 25
        //Layout.bottomMargin: 40

        //Layout.leftMargin: 24
        Layout.bottomMargin: 40
        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

        width: 530
        enabled: layout.isConnected && (hwDeviceModel.selDevice >= 0)

        Component.onCompleted: {
            confirm_but.preferred = true
        }
        function click_enter() {
            if (!confirm_but.enabled) return

            sig_import()
        }
    }

    Keys.onEnterPressed: {
         confirm_but.click_enter()
    }

    Keys.onReturnPressed: {
         confirm_but.click_enter()
    }
}
