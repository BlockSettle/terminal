import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15
import QtQuick.Dialogs 1.3


import "StyledControls"
import "BsStyles"

ColumnLayout  {

    id: layout

    signal sig_import()
    signal sig_full()

    property bool isFileChoosen: false

    height: 511
    width: 580

    spacing: 0

    CustomTitleLabel {
        id: title
        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : title.height
        text: qsTr("Import wallet")
    }

    CustomTextSwitch {
        id: type_switch

        Layout.alignment: Qt.AlignCenter
        Layout.topMargin: 24

        onSig_full_changed: (isFull) => {
            if (isFull === true)
            {
                type_switch.isFullChoosed = false
                layout.sig_full()
            }
        }

        Component.onCompleted: {
            type_switch.isFullChoosed = false
        }
    }

    Image {
        id: dashed_border

        Layout.alignment: Qt.AlignCenter
        Layout.topMargin: 24
        Layout.leftMargin: 25
        Layout.rightMargin: 25
        Layout.preferredHeight: 282
        height: 282

        source: "qrc:/images/file_drop.png"

        Image {
            id: file_icon

            visible: layout.isFileChoosen

            width:12
            height: 16

            anchors.left: parent.left
            anchors.leftMargin: 203
            anchors.verticalCenter: parent.verticalCenter

            source: "qrc:/images/File.png"
        }

        Image {
            id: folder_icon

            visible: !layout.isFileChoosen

            width: 20
            height: 16

            anchors.left: parent.left
            anchors.leftMargin: 200
            anchors.verticalCenter: parent.verticalCenter

            source: "qrc:/images/folder_icon.png"
        }

        Label {
            id: label_file

            visible: layout.isFileChoosen

            anchors.left: parent.left
            anchors.leftMargin: 230
            anchors.verticalCenter: parent.verticalCenter
            color: "#E2E7FF"
            font.pixelSize: 16
            font.family: "Roboto"
            font.weight: Font.Normal
        }

        Label {
            id: label_folder

            visible: !layout.isFileChoosen

            anchors.left: parent.left
            anchors.leftMargin: 225
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Select the file")
            color: "#E2E7FF"
            font.pixelSize: 16
            font.family: "Roboto"
            font.weight: Font.Normal
        }

        FileDialog  {
            id: fileDialog
            visible: false
            onAccepted: {
                label_file.text = basename(fileDialog.fileUrl.toString())
                layout.isFileChoosen = true
                dashed_border.source = "qrc:/images/wallet_file.png"
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                fileDialog.visible = true
            }
        }

    }

    CustomButton {
        id: import_but
        text: qsTr("Import")
        Layout.leftMargin: 25
        Layout.topMargin: 32
        width: 530
        enabled: layout.isFileChoosen

        Component.onCompleted: {
            import_but.preferred = true
        }

        function click_enter() {
            if (!import_but.enabled) return

            layout.sig_import()
        }
    }


    Keys.onEnterPressed: {
         import_but.click_enter()
    }

    Keys.onReturnPressed: {
         import_but.click_enter()
    }

    Label {
        id: spacer
        Layout.fillHeight: true
        Layout.fillWidth: true
    }

    function basename(str)
    {
        return (str.slice(str.lastIndexOf("/")+1))
    }

}
