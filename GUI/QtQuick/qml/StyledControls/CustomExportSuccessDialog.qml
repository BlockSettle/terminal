import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"

Window  {
    id: root

    property alias header: title.text
    property alias export_type: export_type.text
    property alias path_name: file_path.text

    visible: true
    flags: Qt.WindowCloseButtonHint | Qt.FramelessWindowHint | Qt.Dialog
    modality: Qt.WindowModal

    height: 400
    width: 580

    color: "transparent"

    x: mainWindow.x + (mainWindow.width - width)/2
    y: mainWindow.y + (mainWindow.height - height)/2

    Rectangle {
        id: rect

        color: "#191E2A"
        opacity: 1
        radius: 16

        anchors.fill: parent

        border.color : BSStyle.defaultBorderColor
        border.width : 1

        ColumnLayout  {
            id: layout
        
            anchors.fill: parent

            CustomTitleLabel {
                id: title
                Layout.topMargin: 36
                Layout.alignment: Qt.AlignCenter
                Layout.preferredHeight : title.height
                text: qsTr("Success")
            }


            Image {
                id: wallet_icon

                Layout.topMargin: 5
                Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
                Layout.preferredHeight : 120
                Layout.preferredWidth : 120

                source: "qrc:/images/success.png"
                width: 120
                height: 120
            }

            Label {

                id: export_type

                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.topMargin: 8
                Layout.preferredHeight: 16
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignLeft | Qt.AlingTop

                font.pixelSize: 14
                font.family: "Roboto"
                font.weight: Font.Normal

                text: qsTr("Export file:")
                color: BSStyle.wildBlueColor
            }

            Label {
                
                id: file_path

                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.topMargin: 4
                Layout.preferredHeight: 16
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignHCenter | Qt.AlingTop

                font.pixelSize: 14
                font.family: "Roboto"
                font.weight: Font.Normal

                text: "/home"
                color: BSStyle.titanWhiteColor
            }

            CustomButton {
                id: finish_but

                width: 532

                Layout.bottomMargin: 40
                Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

                text: qsTr("Finish")

                preferred: true
                focus:true

                function click_enter() {
                    root.close()
                }
            }

            Keys.onEnterPressed: {
                click_enter()
            }

            Keys.onReturnPressed: {
                click_enter()
            }
        }
    }
}