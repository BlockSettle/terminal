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

    height: 485
    width: 580
    spacing: 0

    CustomTitleLabel {
        id: title
        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : title.height
        text: qsTr("Success")
    }


    Image {
        id: wallet_icon

        Layout.topMargin: 34
        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        Layout.preferredHeight : 120
        Layout.preferredWidth : 120

        source: "qrc:/images/success.png"
        width: 120
        height: 120
    }


    Label {
        id: details

        Layout.topMargin: 26
        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        text: qsTr("Your wallet has successfully been created")
        font.pixelSize: 14
        font.family: "Roboto"
        font.weight: Font.Normal
        color: "#E2E7FF"
    }

    Label {
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    CustomButton {
        id: finish_but

        width: 530

        Layout.bottomMargin: 40
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
