import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"

import wallet.balance 1.0

ColumnLayout  {

    id: layout

    signal sig_continue(signature: var)
    signal sig_simple()

    height: 748
    width: 1132
    spacing: 0

    property var tempRequest: null

    RowLayout {

        Layout.fillWidth: true
        Layout.preferredHeight : 34

        Button {
            id: simple_but

            Layout.leftMargin: 24
            Layout.alignment: Qt.AlignLeft | Qt.AlingVCenter

            activeFocusOnTab: true

            font.pixelSize: 13
            font.family: "Roboto"
            font.weight: Font.Normal
            palette.buttonText: "#7A88B0"

            text: qsTr("Simple")

            icon.color: "transparent"
            icon.source: "qrc:/images/advanced_icon.png"
            icon.width: 16
            icon.height: 16

            background: Rectangle {
                implicitWidth: 100
                implicitHeight: 34
                color: "transparent"

                radius: 14

                border.color: "#3C435A"
                border.width: 1

            }

            onClicked: {
               layout.sig_simple()
            }
        }

        CustomTitleLabel {
            id: title

            Layout.leftMargin: 378
            Layout.alignment: Qt.AlignLeft | Qt.AlingVCenter

            text: qsTr("Send Bitcoin")
        }

        Label {
            Layout.fillWidth: true
            Layout.preferredHeight : 34
        }

    }

    RowLayout {

        id: rects_row

        Layout.fillWidth: true
        Layout.preferredHeight : 580
        Layout.topMargin: 20

        spacing: 12

        Rectangle {
            id: inputs_rect

            Layout.leftMargin: 24
            Layout.alignment: Qt.AlignLeft | Qt.AlingVCenter

            width: 536
            height: 580
            color: "transparent"

            radius: 16

            border.color: "#3C435A"
            border.width: 1

            ColumnLayout  {
                id: inputs_layout

                anchors.fill: parent

                spacing: 0

                Label {
                    id: inputs_title

                    Layout.leftMargin: 16
                    Layout.topMargin: 16
                    Layout.alignment: Qt.AlignLeft | Qt.AlingTop

                    text: qsTr("Inputs")

                    height : 19
                    color: "#E2E7FF"
                    font.pixelSize: 16
                    font.family: "Roboto"
                    font.weight: Font.Medium
                }

                WalletsComboBox {

                    id: from_wallet_combo

                    Layout.leftMargin: 16
                    Layout.topMargin: 16
                    Layout.alignment: Qt.AlignLeft | Qt.AlingTop

                    width: 504
                    height: 70

                    onActivated: {
                        //I dont understand why but acceptableInput dont work...
                        var amount_max = getWalletData(from_wallet_combo.currentIndex, WalletBalance.TotalRole)
                        var cur_value = parseFloat(amount_input.input_text)
                        var bottom = 0
                        var top = tempRequest.maxAmount
                        if(cur_value < bottom || cur_value > top)
                        {
                            amount_input.input_text = amount_max
                        }

                        if (rec_addr_input.isValid) {
                            var fpb = parseFloat(fee_suggest_combo.currentValue)
                            tempRequest = bsApp.createTXSignRequest(from_wallet_combo.currentIndex
                                        , [rec_addr_input.input_text], [], (fpb > 0) ? fpb : 1.0)
                        }
                    }
                }

                FeeSuggestComboBox {

                    id: fee_suggest_combo

                    Layout.leftMargin: 16
                    Layout.topMargin: 10
                    Layout.alignment: Qt.AlignLeft | Qt.AlingTop

                    width: 504
                    height: 70

                    onCurrentIndexChanged: {
                        bsApp.getUTXOsForWallet(from_wallet_combo.currentIndex)
                        txOutputsModel.clearOutputs()
                    }
                }


                Rectangle {

                    height: 1

                    Layout.fillWidth: true
                    Layout.topMargin: 196
                    Layout.alignment: Qt.AlignLeft | Qt.AlingTop

                    color: "#3C435A"
                }

                Label {
                    Layout.fillWidth: true
                    Layout.fillHeight : true
                }
            }
        }

        Rectangle {
            id: outputs_rect

            Layout.rightMargin: 24
            Layout.alignment: Qt.AlignRight | Qt.AlingVCenter

            width: 536
            height: 580
            color: "transparent"

            radius: 16

            border.color: "#3C435A"
            border.width: 1

            ColumnLayout  {
                id: outputs_layout

                anchors.fill: parent

                spacing: 0

                Label {
                    id: outputs_title

                    Layout.leftMargin: 16
                    Layout.topMargin: 16
                    Layout.alignment: Qt.AlignLeft | Qt.AlingTop

                    text: qsTr("Outputs")

                    height : 19
                    color: "#E2E7FF"
                    font.pixelSize: 16
                    font.family: "Roboto"
                    font.weight: Font.Medium
                }

                RecvAddrTextInput {

                    id: rec_addr_input

                    Layout.leftMargin: 16
                    Layout.topMargin: 16
                    Layout.alignment: Qt.AlignLeft | Qt.AlingTop

                    width: 504
                    height: 70

                    wallets_current_index: from_wallet_combo.currentIndex

                    onFocus_next: {
                        amount_input.setActiveFocus()
                    }


                    function createTempRequest() {
                        var fpb = parseFloat(fee_suggest_combo.currentValue)
                        tempRequest = bsApp.createTXSignRequest(from_wallet_combo.currentIndex
                                    , [rec_addr_input.input_text], [], (fpb > 0) ? fpb : 1.0)
                    }
                }

                AmountInput {

                    id: amount_input

                    Layout.leftMargin: 16
                    Layout.topMargin: 10
                    Layout.alignment: Qt.AlignLeft | Qt.AlingTop

                    width: 504
                    height: 70
                }

                CustomTextEdit {

                    id: comment_input

                    Layout.leftMargin: 16
                    Layout.topMargin: 10
                    Layout.alignment: Qt.AlignLeft | Qt.AlingTop

                    Layout.preferredHeight : 90
                    Layout.preferredWidth: 504

                    //aliases
                    title_text: qsTr("Comment")

                    onTabNavigated: continue_but.forceActiveFocus()
                    onBackTabNavigated: fee_suggest_combo.forceActiveFocus()
                }

                CustomButton {
                    id: include_output_but
                    text: qsTr("Include Output")

                    Layout.leftMargin: 16
                    Layout.topMargin: 16
                    Layout.alignment: Qt.AlignLeft | Qt.AlingTop

                    enabled: rec_addr_input.isValid && rec_addr_input.input_text.length
                             && parseFloat(amount_input.input_text) !== 0 && amount_input.input_text.length

                    icon.source: "qrc:/images/plus.svg"
                    icon.color: include_output_but.enabled ? "#45A6FF" : BSStyle.buttonsDisabledTextColor

                    width: 504

                    Component.onCompleted: {
                        include_output_but.preferred = false
                    }

                    function click_enter() {
                        if (!include_output_but.enabled) return

                        txOutputsModel.addOutput(rec_addr_input.input_text, amount_input.input_text)
                    }
                }

                Rectangle {

                    height: 1

                    Layout.fillWidth: true
                    Layout.topMargin: 30
                    Layout.alignment: Qt.AlignLeft | Qt.AlingTop

                    color: "#3C435A"
                }

                CustomTableView {

                    Layout.fillWidth: true
                    Layout.fillHeight : true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16

                    model: txOutputsModel
                    columnWidths: [0.744, 0.20, 0.056]

                    text_header_size: 12
                    cell_text_size: 13
                    copy_button_column_index: -1
                    delete_button_column_index: 2
                    left_first_header_padding: 0

                    onDeleteRequested: (row) =>
                    {
                        txOutputsModel.delOutput(row)
                    }
                }

                TreeView {
                    TableViewColumn {
                        title: "Name"
                        role: "fileName"
                        width: 300
                    }
                    TableViewColumn {
                        title: "Permissions"
                        role: "filePermissions"
                        width: 100
                    }
                    model: fileSystemModel
                }

            }
        }
    }


    Label {
        Layout.fillWidth: true
        Layout.fillHeight : true
    }


    CustomButton {
        id: broadcast_but

        enabled: false

        width: 1084

        Layout.bottomMargin: 40
        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

        text: qsTr("Broadcast")

        Component.onCompleted: {
            broadcast_but.preferred = true
        }

        function click_enter() {

        }

    }


    function init()
    {
        rec_addr_input.setActiveFocus()

        //we need set first time currentIndex to 0
        //only after we will have signal rowchanged
        if (fee_suggest_combo.currentIndex >= 0)
            fee_suggest_combo.currentIndex = 0
        if (from_wallet_combo.currentIndex >= 0)
            from_wallet_combo.currentIndex = overviewWalletIndex

        amount_input.input_text = ""
        comment_input.input_text = ""
        rec_addr_input.input_text = ""

        bsApp.getUTXOsForWallet(from_wallet_combo.currentIndex)
        txOutputsModel.clearOutputs()
    }
}
