import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"

ColumnLayout  {

    id: layout

    property var txSignRequest: null

    property int time_progress

    signal sig_broadcast()
    signal sig_time_finished()

    height: 554
    width: 580
    spacing: 0

    CustomTitleLabel {
        id: title
        Layout.topMargin: 6
        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        Layout.preferredHeight : title.height
        text: qsTr("Sign Transaction")
    }

    Label {
        Layout.fillWidth: true
        height: 24
    }

    Rectangle {

        id: output_rect

        width: 532
        height: 82

        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter

        radius: 14

        color: "#32394F"

        Label {

            id: out_addr_title

            anchors.top: parent.top
            anchors.topMargin: 18
            anchors.left: parent.left
            anchors.leftMargin: 20

            text: qsTr("Output address:")

            color: "#45A6FF"

            font.pixelSize: 14
            font.family: "Roboto"
            font.weight: Font.Normal
        }

        Label {

            id: out_amount_title

            anchors.top: out_addr_title.bottom
            anchors.topMargin: 14
            anchors.left: parent.left
            anchors.leftMargin: 20

            text: qsTr("Output amount:")

            color: "#45A6FF"

            font.pixelSize: 14
            font.family: "Roboto"
            font.weight: Font.Normal
        }

        Label {

            id: out_addr

            anchors.top: parent.top
            anchors.topMargin: 18
            anchors.right: parent.right
            anchors.rightMargin: 20

            text: txSignRequest !== null ? txSignRequest.outputAddresses[0] : ""

            color: "#FFFFFF"

            font.pixelSize: 14
            font.family: "Roboto"
            font.weight: Font.Normal
        }

        Label {

            id: out_amount

            anchors.top: out_addr.bottom
            anchors.topMargin: 14
            anchors.right: parent.right
            anchors.rightMargin: 20

            text: txSignRequest !== null ? txSignRequest.outputAmount : ""

            color: "#FFFFFF"

            font.pixelSize: 14
            font.family: "Roboto"
            font.weight: Font.Normal
        }

    }

    Rectangle {

        id: input_rect

        width: 532
        height: 188

        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter

        color: "transparent"

        Label {

            id: in_amount_title

            anchors.top: parent.top
            anchors.topMargin: 18
            anchors.left: parent.left
            anchors.leftMargin: 20

            text: qsTr("Input amount:")

            color: "#7A88B0"

            font.pixelSize: 14
            font.family: "Roboto"
            font.weight: Font.Normal
        }

        Label {

            id: in_amount

            anchors.top: parent.top
            anchors.topMargin: 18
            anchors.right: parent.right
            anchors.rightMargin: 20

            text: txSignRequest !== null ? txSignRequest.inputAmount : ""

            color: "#E2E7FF"

            font.pixelSize: 14
            font.family: "Roboto"
            font.weight: Font.Normal
        }

        Label {

            id: return_amount_title

            anchors.top: in_amount_title.bottom
            anchors.topMargin: 15
            anchors.left: parent.left
            anchors.leftMargin: 20

            text: qsTr("Return amount:")

            color: "#7A88B0"

            font.pixelSize: 14
            font.family: "Roboto"
            font.weight: Font.Normal
        }

        Label {

            id: return_amount

            anchors.top: in_amount.bottom
            anchors.topMargin: 15
            anchors.right: parent.right
            anchors.rightMargin: 20

            text: txSignRequest !== null ? txSignRequest.returnAmount : ""

            color: "#E2E7FF"

            font.pixelSize: 14
            font.family: "Roboto"
            font.weight: Font.Normal
        }

        Label {

            id: transaction_fee_title

            anchors.top: return_amount_title.bottom
            anchors.topMargin: 15
            anchors.left: parent.left
            anchors.leftMargin: 20

            text: qsTr("Transaction fee:")

            color: "#7A88B0"

            font.pixelSize: 14
            font.family: "Roboto"
            font.weight: Font.Normal
        }

        Label {

            id: transaction_fee

            anchors.top: return_amount.bottom
            anchors.topMargin: 15
            anchors.right: parent.right
            anchors.rightMargin: 20

            text: txSignRequest !== null ? txSignRequest.fee : ""

            color: "#E2E7FF"

            font.pixelSize: 14
            font.family: "Roboto"
            font.weight: Font.Normal
        }

        Label {

            id: transaction_size_title

            anchors.top: transaction_fee_title.bottom
            anchors.topMargin: 15
            anchors.left: parent.left
            anchors.leftMargin: 20

            text: qsTr("Transaction size:")

            color: "#7A88B0"

            font.pixelSize: 14
            font.family: "Roboto"
            font.weight: Font.Normal
        }

        Label {

            id: transaction_size

            anchors.top: transaction_fee.bottom
            anchors.topMargin: 15
            anchors.right: parent.right
            anchors.rightMargin: 20

            text: txSignRequest !== null ? txSignRequest.txSize : ""

            color: "#E2E7FF"

            font.pixelSize: 14
            font.family: "Roboto"
            font.weight: Font.Normal
        }

        Label {

            id: fee_per_byte_title

            anchors.top: transaction_size_title.bottom
            anchors.topMargin: 15
            anchors.left: parent.left
            anchors.leftMargin: 20

            text: qsTr("Fee-per-byte:")

            color: "#7A88B0"

            font.pixelSize: 14
            font.family: "Roboto"
            font.weight: Font.Normal
        }

        Label {

            id: fee_per_byte

            anchors.top: transaction_size.bottom
            anchors.topMargin: 15
            anchors.right: parent.right
            anchors.rightMargin: 20

            text: txSignRequest !== null ? txSignRequest.feePerByte : ""

            color: "#E2E7FF"

            font.pixelSize: 14
            font.family: "Roboto"
            font.weight: Font.Normal
        }

    }

    PasswordWithTimer {
        id: password

        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        Layout.topMargin: 10

        width: 532

        time_progress: layout.time_progress
    }

    Label {
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    CustomButton {
        id: broadcast_but
        text: txSignRequest !== null ? (txSignRequest.hasError ? txSignRequest.errorText : qsTr("Broadcast")) : ""
        width: 532

        Layout.bottomMargin: 40
        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

        enabled: txSignRequest !== null ?
          (!txSignRequest.hasError && ((txSignRequest.isHWW && txSignRequest.isHWWready) || password.value.length)) :
          false

        preferred: true

        function click_enter() {
            bsApp.signAndBroadcast(txSignRequest, password.value)
            password.value = ""
            sig_broadcast()
        }
    }

    Keys.onEnterPressed: {
        broadcast_but.click_enter()
    }

    Keys.onReturnPressed: {
        broadcast_but.click_enter()
    }

    Timer {
        id: timer

        interval: 1000
        running: true
        repeat: true
        onTriggered: {
            layout.time_progress = layout.time_progress - 1

            if (time_progress === 0)
            {
                running = false
                password.value = ""
                sig_time_finished()
            }
        }
    }

    function init()
    {
        password.value = ""
        time_progress = 120
        password.setActiveFocus()
        timer.running = true
    }

}
