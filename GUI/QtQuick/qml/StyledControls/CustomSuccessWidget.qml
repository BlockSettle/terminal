import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"

ColumnLayout  {

    id: layout

    property alias details_text: details.text
    property alias details_font_size: details.font.pixelSize
    property alias details_font_weight: details.font.weight

    signal sig_finish()

    height: BSSizes.applyScale(485)
    width: BSSizes.applyScale(580)
    spacing: 0

    CustomTitleLabel {
        id: title
        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : title.height
        text: qsTr("Success")
    }


    Image {
        id: wallet_icon

        Layout.topMargin: BSSizes.applyScale(34)
        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        Layout.preferredHeight : BSSizes.applyScale(120)
        Layout.preferredWidth : BSSizes.applyScale(120)

        source: "qrc:/images/success.png"
        width: BSSizes.applyScale(120)
        height: BSSizes.applyScale(120)
    }


    Label {
        id: details

        Layout.topMargin: BSSizes.applyScale(26)
        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        text: qsTr("Your wallet has successfully been created")
        font.pixelSize: BSSizes.applyScale(14)
        font.family: "Roboto"
        font.weight: Font.Normal
        color: "#E2E7FF"
        wrapMode: Text.Wrap
        Layout.maximumWidth: parent.width
        Layout.leftMargin: BSSizes.applyScale(10)
        Layout.rightMargin: BSSizes.applyScale(10)
    }

    Label {
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    CustomButton {
        id: finish_but

        width: BSSizes.applyScale(530)

        Layout.bottomMargin: BSSizes.applyScale(40)
        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

        text: qsTr("Finish")

        preferred: true
        focus:true

        function click_enter() {
            sig_finish()
        }
    }

    Keys.onEnterPressed: {
        click_enter()
    }

    Keys.onReturnPressed: {
        click_enter()
    }

    function init() {
        finish_but.setActiveFocus()
    }
}
