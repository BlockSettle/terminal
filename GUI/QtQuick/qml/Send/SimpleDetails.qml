import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"

import wallet.balance 1.0

ColumnLayout  {

    id: layout

    signal sig_continue(signature: var)
    signal sig_advanced()

    height: 554
    width: 600
    spacing: 0

    property var tempRequest: null

    RowLayout {

        Layout.fillWidth: true
        Layout.preferredHeight : 34

        Button {
            id: advanced_but

            Layout.leftMargin: 24
            Layout.alignment: Qt.AlignLeft | Qt.AlingVCenter

            activeFocusOnTab: true

            font.pixelSize: 13
            font.family: "Roboto"
            font.weight: Font.Normal
            palette.buttonText: "#7A88B0"

            text: qsTr("Advanced")


            icon.color: "transparent"
            icon.source: "qrc:/images/advanced_icon.png"
            icon.width: 16
            icon.height: 16

            background: Rectangle {
                implicitWidth: 116
                implicitHeight: 34
                color: "transparent"

                radius: 14

                border.color: "#3C435A"
                border.width: 1

            }

            onClicked: {
               layout.sig_advanced()
            }
        }

        CustomTitleLabel {
            id: title

            Layout.leftMargin: 104
            Layout.alignment: Qt.AlignLeft | Qt.AlingVCenter

            text: qsTr("Send Bitcoin")
        }

        Label {
            Layout.fillWidth: true
            Layout.preferredHeight : 34
        }

    }

    RecvAddrTextInput {

        id: rec_addr_input

        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : 70
        Layout.preferredWidth: 552
        Layout.topMargin: 23

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

        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : 70
        Layout.preferredWidth: 552
        Layout.topMargin: 10
    }

    RowLayout {

        Layout.fillWidth: true
        Layout.preferredHeight : 70
        Layout.topMargin: 10

        WalletsComboBox {

            id: from_wallet_combo

            Layout.leftMargin: 24
            Layout.alignment: Qt.AlignLeft | Qt.AlingVCenter

            width: 271

            onActivated: (index_act) => {
                if (rec_addr_input.isValid) {
                    var fpb = parseFloat(fee_suggest_combo.currentValue)
                    tempRequest = bsApp.createTXSignRequest(index_act
                                , [rec_addr_input.input_text], [], (fpb > 0) ? fpb : 1.0)
                }

                //I dont understand why but acceptableInput dont work...
                var cur_value = parseFloat(amount_input.input_text)
                var bottom = 0
                var top = tempRequest.maxAmount
                if(cur_value < bottom || cur_value > top)
                {
                    amount_input.input_text = tempRequest.maxAmount
                }
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.preferredHeight: 70
        }

        FeeSuggestComboBox {

            id: fee_suggest_combo

            Layout.rightMargin: 24
            Layout.alignment: Qt.AlignRight | Qt.AlingVCenter

            width: 271
        }
    }

    CustomTextEdit {

        id: comment_input

        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : 90
        Layout.preferredWidth: 552
        Layout.topMargin: 10

        //aliases
        title_text: qsTr("Comment")

        onTabNavigated: continue_but.forceActiveFocus()
        onBackTabNavigated: fee_suggest_combo.forceActiveFocus()
    }

    Label {
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    CustomButton {
        id: continue_but

        enabled: rec_addr_input.isValid && rec_addr_input.input_text.length
                 && parseFloat(amount_input.input_text) !== 0 && amount_input.input_text.length

        width: 552

        Layout.bottomMargin: 40
        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

        text: qsTr("Continue")

        Component.onCompleted: {
            continue_but.preferred = true
        }

        function click_enter() {
            layout.sig_continue( bsApp.createTXSignRequest(from_wallet_combo.currentIndex
                , [rec_addr_input.input_text], [parseFloat(amount_input.input_text)]
                , parseFloat(fee_suggest_combo.currentValue), comment_input.input_text))
        }
    }


    Keys.onEnterPressed: {
        continue_but.click_enter()
    }

    Keys.onReturnPressed: {
        continue_but.click_enter()
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
    }
}

