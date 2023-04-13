import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"

ColumnLayout  {

    id: layout

    signal sig_skip()
    signal sig_not_skip()

    height: BSSizes.applyScale(485)
    width: BSSizes.applyScale(580)

    spacing: 0

    CustomTitleLabel {
        id: title
        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : title.height
        text: qsTr("Verify your seed")
    }

    Image {
        id: warning_icon

        Layout.alignment: Qt.AlignHCenter | Qt.AlignTop
        Layout.topMargin: BSSizes.applyScale(24)
        Layout.preferredHeight : BSSizes.applyScale(44)
        Layout.preferredWidth : BSSizes.applyScale(44)

        source: "qrc:/images/warning_icon.png"
        width: BSSizes.applyScale(44)
        height: BSSizes.applyScale(44)
    }

    Label {
        id: warning_description

        text: qsTr("Are you sure you do not want to verify your seed?")

        Layout.alignment: Qt.AlignHCenter | Qt.AlignTop

        Layout.topMargin: BSSizes.applyScale(16)
        Layout.preferredHeight : BSSizes.applyScale(16)

        color: "#E2E7FF"
        font.pixelSize: BSSizes.applyScale(14)
        font.family: "Roboto"
        font.weight: Font.Normal
    }

    Label {
        id: spacer
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    RowLayout {
        id: row
        spacing: BSSizes.applyScale(10)

        //Layout.leftMargin: 24
        Layout.bottomMargin: BSSizes.applyScale(40)
        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

        CustomButton {
            id: no_but
            text: qsTr("No")
            width: BSSizes.applyScale(261)

            preferred: false

            function click_enter() {
                layout.sig_not_skip()
            }

        }

        CustomButton {
            id: skip_but
            text: qsTr("Yes, Skip")
            width: BSSizes.applyScale(261)

            preferred: true

            function click_enter() {
                layout.sig_skip()
            }
        }
    }

    function init()
    {
        skip_but.forceActiveFocus()
    }
}
