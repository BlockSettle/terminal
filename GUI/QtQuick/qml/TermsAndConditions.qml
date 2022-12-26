import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "StyledControls"
import "BsStyles"

ColumnLayout  {

    id: layout

    signal sig_continue()

    height: 481
    width: 580
    implicitHeight: 481
    implicitWidth: 580

    spacing: 0

    CustomTitleLabel {
        id: title
        Layout.alignment: Qt.AlignLeft
        Layout.leftMargin: 24
        Layout.preferredHeight : title.height
        text: "Terms and conditions"
    }

    Label {
        Layout.fillWidth: true
        height: 24
    }

    ScrollView {
        id: scroll
        Layout.alignment: Qt.AlignLeft
        Layout.leftMargin: 24
        implicitWidth: 532
        implicitHeight: 340

        ScrollBar.vertical.policy: ScrollBar.AlwaysOn

        clip: true

        TextArea {
            id: edit

            width: parent.width
            height: parent.height
            font.pixelSize: 16
            font.family: "Roboto"
            font.weight: Font.Normal
            leftPadding: 0
            rightPadding: 24
            color: "#E2E7FF"
            text: "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.\n\nSed ut perspiciatis unde omnis iste natus error sit voluptatem accusantium doloremque laudantium, totam rem aperiam, eaque ipsa quae ab illo inventore veritatis et quasi architecto beatae vitae dicta sunt explicabo. Nemo enim ipsam voluptatem quia voluptas sit aspernatur aut odit aut fugit, sed quia consequuntur magni dolores eos qui ratione voluptatem sequi nesciunt.\n\nNeque porro quisquam est, qui dolorem ipsum quia dolor sit amet, consectetur, adipisci velit, sed quia non numquam eius modi tempora incidunt ut labore et dolore magnam aliquam quaerat voluptatem. Ut enim ad minima veniam, quis nostrum exercitationem ullam corporis suscipit\n\nLorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.\n\nSed ut perspiciatis unde omnis iste natus error sit voluptatem accusantium doloremque laudantium, totam rem aperiam, eaque ipsa quae ab illo inventore veritatis et quasi architecto beatae vitae dicta sunt explicabo. Nemo enim ipsam voluptatem quia voluptas sit aspernatur aut odit aut fugit, sed quia consequuntur magni dolores eos qui ratione voluptatem sequi nesciunt.\n\nNeque porro quisquam est, qui dolorem ipsum quia dolor sit amet, consectetur, adipisci velit, sed quia non numquam eius modi tempora incidunt ut labore et dolore magnam aliquam quaerat voluptatem. Ut enim ad minima veniam, quis nostrum exercitationem ullam corporis suscipit"
            selectByMouse: true
            wrapMode: TextEdit.WordWrap
            readOnly: true

            background: Rectangle {
                color: "transparent"
                radius: 4
            }

        }
    }

    Rectangle {
        Layout.fillWidth: true
        height: 1
        color: "#3C435A"
    }

    Label {
        Layout.fillWidth: true
        height: 24
    }

    CustomButton {
        id: continue_but
        text: qsTr("Continue")
        Layout.leftMargin: 24
        width: 532

        Component.onCompleted: {
            continue_but.preferred = true
        }
        onClicked: {
            sig_continue()
        }
    }

    Label {
        Layout.fillWidth: true
        height: 24
    }
}

