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
    property bool isRBF: false
    property bool isCPFP: false

    signal sig_broadcast()
    signal sig_time_finished()

    height: BSSizes.applyWindowHeightScale(748)
    width: BSSizes.applyWindowWidthScale(1132)
    spacing: 0

    Connections
    {
        target:bsApp
        function onSuccessTx()
        {
            if (!layout.visible) {
                return
            }
            
            sig_broadcast()
        }
        function onFailedTx(error)
        {
            if (!layout.visible) {
                return
            }
            
            fail_dialog.fail = error
            fail_dialog.show()
            fail_dialog.raise()
            fail_dialog.requestActivate()
        }
    }

    CustomFailDialog {
        id: fail_dialog
        header: "Failed to send"
        visible: false;
    }

    CustomTitleLabel {
        id: title
        Layout.topMargin: 6
        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        Layout.preferredHeight : title.height
        text: qsTr("Sign Transaction")
    }


    RowLayout {
        id: rects_row

        Layout.fillWidth: true
        Layout.preferredHeight : BSSizes.applyScale(312)
        Layout.topMargin: BSSizes.applyScale(24)

        spacing: BSSizes.applyScale(20)

        Rectangle {
            id: inputs_rect

            Layout.leftMargin: BSSizes.applyScale(22)
            Layout.alignment: Qt.AlignLeft | Qt.AlingVCenter

            width: BSSizes.applyScale(532)
            height: BSSizes.applyScale(312)
            color: "transparent"

            radius: BSSizes.applyScale(14)

            border.color: BSStyle.defaultBorderColor
            border.width: BSSizes.applyScale(1)

            CustomTableView {
                id: table_sel_inputs

                width: parent.width - BSSizes.applyScale(28)
                height: parent.height - BSSizes.applyScale(24)
                anchors.centerIn: parent

                model: txSignRequest !== null ? txSignRequest.inputs : []
                columnWidths: [0.7, 0.1, 0, 0.2]

                copy_button_column_index: -1
                has_header: false

                function get_text_left_padding(row, column)
                {
                    return (row === 0 && column === 0) ? 0 : left_text_padding
                }

                function get_data_color(row, column)
                {
                    return row === 0 ? "#45A6FF" : null
                }
            }
        }

        Rectangle {
            id: outputs_rect

            Layout.rightMargin: BSSizes.applyScale(22)
            Layout.alignment: Qt.AlignRight | Qt.AlingVCenter

            width: BSSizes.applyScale(532)
            height: BSSizes.applyScale(312)
            color: "#32394F"

            radius: BSSizes.applyScale(14)

            CustomTableView {
                id: table_outputs

                width: parent.width - BSSizes.applyScale(28)
                height: parent.height - BSSizes.applyScale(24)
                anchors.centerIn: parent

                model: txSignRequest !== null ? txSignRequest.outputs : []
                columnWidths: [0.7, 0.1, 0.20]

                copy_button_column_index: -1
                delete_button_column_index: -1
                has_header: false

                function get_text_left_padding(row, column)
                {
                    return (row === 0 && column === 0) ? 0 : left_text_padding
                }

                function get_data_color(row, column)
                {
                    return row === 0 ? "#45A6FF" : null
                }
            }
        }
    }

    Rectangle {
        id: details_rect

        Layout.alignment: Qt.AlignHCenter | Qt.AlingTop
        Layout.preferredHeight : BSSizes.applyScale(100)
        Layout.topMargin: BSSizes.applyScale(20)

        width: BSSizes.applyScale(1084)
        height: BSSizes.applyScale(100)
        color: "transparent"

        radius: BSSizes.applyScale(14)

        border.color: BSStyle.defaultBorderColor
        border.width: BSSizes.applyScale(1)

        Label {

            id: in_amount_title

            anchors.top: parent.top
            anchors.topMargin: BSSizes.applyScale(16)
            anchors.left: parent.left
            anchors.leftMargin: BSSizes.applyScale(16)

            text: qsTr("Input amount:")

            color: "#7A88B0"

            font.pixelSize: BSSizes.applyScale(14)
            font.family: "Roboto"
            font.weight: Font.Normal
        }

        Label {

            id: in_amount

            anchors.top: parent.top
            anchors.topMargin: BSSizes.applyScale(16)
            anchors.right: parent.horizontalCenter
            anchors.rightMargin: BSSizes.applyScale(24)

            text: txSignRequest !== null ? txSignRequest.inputAmount : ""

            color: "#E2E7FF"

            font.pixelSize: BSSizes.applyScale(14)
            font.family: "Roboto"
            font.weight: Font.Normal
        }

        Label {

            id: return_amount_title

            anchors.top: in_amount_title.bottom
            anchors.topMargin: BSSizes.applyScale(10)
            anchors.left: parent.left
            anchors.leftMargin: BSSizes.applyScale(16)

            text: qsTr("Return amount:")

            color: "#7A88B0"

            font.pixelSize: BSSizes.applyScale(14)
            font.family: "Roboto"
            font.weight: Font.Normal
        }

        Label {

            id: return_amount

            anchors.top: in_amount.bottom
            anchors.topMargin: BSSizes.applyScale(10)
            anchors.right: parent.horizontalCenter
            anchors.rightMargin: BSSizes.applyScale(24)

            text: txSignRequest !== null ? txSignRequest.returnAmount : ""

            color: "#E2E7FF"

            font.pixelSize: BSSizes.applyScale(14)
            font.family: "Roboto"
            font.weight: Font.Normal
        }

        Label {

            id: transaction_fee_title

            anchors.top: return_amount_title.bottom
            anchors.topMargin: BSSizes.applyScale(10)
            anchors.left: parent.left
            anchors.leftMargin: BSSizes.applyScale(16)

            text: qsTr("Transaction fee:")

            color: "#7A88B0"

            font.pixelSize: BSSizes.applyScale(14)
            font.family: "Roboto"
            font.weight: Font.Normal
        }

        Label {

            id: transaction_fee

            anchors.top: return_amount.bottom
            anchors.topMargin: BSSizes.applyScale(10)
            anchors.right: parent.horizontalCenter
            anchors.rightMargin: BSSizes.applyScale(24)

            text: txSignRequest !== null ? txSignRequest.fee : ""

            color: "#E2E7FF"

            font.pixelSize: BSSizes.applyScale(14)
            font.family: "Roboto"
            font.weight: Font.Normal
        }

        Label {

            id: transaction_size_title

            anchors.top: parent.top
            anchors.topMargin: BSSizes.applyScale(16)
            anchors.left: parent.horizontalCenter
            anchors.leftMargin: BSSizes.applyScale(24)

            text: qsTr("Transaction size:")

            color: "#7A88B0"

            font.pixelSize: BSSizes.applyScale(14)
            font.family: "Roboto"
            font.weight: Font.Normal
        }

        Label {

            id: transaction_size

            anchors.top: parent.top
            anchors.topMargin: BSSizes.applyScale(16)
            anchors.right: parent.right
            anchors.rightMargin: BSSizes.applyScale(16)

            text: txSignRequest !== null ? txSignRequest.txSize : ""

            color: "#E2E7FF"

            font.pixelSize: BSSizes.applyScale(14)
            font.family: "Roboto"
            font.weight: Font.Normal
        }

        Label {

            id: fee_per_byte_title

            anchors.top: transaction_size_title.bottom
            anchors.topMargin: BSSizes.applyScale(10)
            anchors.left: parent.horizontalCenter
            anchors.leftMargin: BSSizes.applyScale(24)

            text: qsTr("Fee-per-byte:")

            color: "#7A88B0"

            font.pixelSize: BSSizes.applyScale(14)
            font.family: "Roboto"
            font.weight: Font.Normal
        }

        Label {

            id: fee_per_byte

            anchors.top: transaction_size.bottom
            anchors.topMargin: BSSizes.applyScale(10)
            anchors.right: parent.right
            anchors.rightMargin: BSSizes.applyScale(16)

            text: txSignRequest !== null ? txSignRequest.feePerByte : ""

            color: "#E2E7FF"

            font.pixelSize: BSSizes.applyScale(14)
            font.family: "Roboto"
            font.weight: Font.Normal
        }

        Label {

            id: total_spent_title

            anchors.top: fee_per_byte_title.bottom
            anchors.topMargin: BSSizes.applyScale(10)
            anchors.left: parent.horizontalCenter
            anchors.leftMargin: BSSizes.applyScale(24)

            text: qsTr("Total spent:")

            color: "#7A88B0"

            font.pixelSize: BSSizes.applyScale(14)
            font.family: "Roboto"
            font.weight: Font.Normal
        }

        Label {

            id: total_spent

            anchors.top: fee_per_byte.bottom
            anchors.topMargin: BSSizes.applyScale(10)
            anchors.right: parent.right
            anchors.rightMargin: BSSizes.applyScale(16)

            text: txSignRequest !== null ? txSignRequest.outputAmount : ""

            color: "#E2E7FF"

            font.pixelSize: BSSizes.applyScale(14)
            font.family: "Roboto"
            font.weight: Font.Normal
        }
    }

    PasswordWithTimer {
        id: password

        Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        Layout.topMargin: BSSizes.applyScale(10)

        width: BSSizes.applyScale(530)

        time_progress: layout.time_progress

        onEnterPressed: {
            broadcast_but.click_enter()
        }
        onReturnPressed: {
            broadcast_but.click_enter()
        }
    }

    Label {
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    CustomButton {
        id: broadcast_but
        text: txSignRequest !== null ? (txSignRequest.hasError ? txSignRequest.errorText : qsTr("Broadcast")) : ""
        width: BSSizes.applyScale(1084)

        Layout.bottomMargin: BSSizes.applyScale(40)
        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

        enabled: txSignRequest !== null ? 
            (!txSignRequest.hasError && ((txSignRequest.isHWW && txSignRequest.isHWWready) || password.value.length)) :
            false

        preferred: true

        function click_enter() {
            bsApp.signAndBroadcast(txSignRequest, password.value)
            password.value = ""
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
            if (layout.time_progress != 0) {
                layout.time_progress = layout.time_progress - 1
            }

            if (time_progress === 0 && !fail_dialog.visible)
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
