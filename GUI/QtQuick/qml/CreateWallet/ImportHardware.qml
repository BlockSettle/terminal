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

    height: BSSizes.applyScale(485)
    width: BSSizes.applyScale(580)

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
        Layout.topMargin: BSSizes.applyScale(24)
        Layout.preferredHeight : BSSizes.applyScale(44)
        Layout.preferredWidth : BSSizes.applyScale(44)

        width: BSSizes.applyScale(44)
        height: BSSizes.applyScale(44)

        source: layout.isConnected ? "qrc:/images/USB_icon_conn.png" : "qrc:/images/USB_icon_disconn.png"
    }


    Label {
        id: subtitle
        visible: hwDeviceModel.empty
        Layout.alignment: Qt.AlignCenter
        Layout.topMargin: BSSizes.applyScale(16)
        Layout.preferredHeight : BSSizes.applyScale(16)
        text: qsTr("Connect your wallet")
        color: "#E2E7FF"
        font.pixelSize: BSSizes.applyScale(14)
        font.family: "Roboto"
        font.weight: Font.Normal
    }

    TableView {
        visible: !hwDeviceModel.empty
        width: BSSizes.applyScale(800)
        height: BSSizes.applyScale(200)
        columnSpacing: BSSizes.applyScale(1)
        rowSpacing: BSSizes.applyScale(1)
        clip: true
        ScrollIndicator.horizontal: ScrollIndicator { }
        ScrollIndicator.vertical: ScrollIndicator { }
        model: hwDeviceModel
        delegate: Rectangle {
            implicitWidth: BSSizes.applyScale(750)
            implicitHeight: BSSizes.applyScale(20)
            border.color: "black"
            border.width: 1
            color: 'darkslategrey'
            Text {
                text: label
                font.pointSize: BSSizes.applyScale(12)
                color: 'lightgrey'
                anchors.centerIn: parent
            }
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    //TODO: select device
                }
            }
        }
    }

    Label {
        id: spacer
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    CustomButton {
        id: confirm_but
        text: qsTr("Import")

        Layout.bottomMargin: BSSizes.applyScale(40)
        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

        width: BSSizes.applyScale(530)
        enabled: /*layout.isConnected &&*/ (hwDeviceModel.selDevice >= 0)
        preferred: true

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
